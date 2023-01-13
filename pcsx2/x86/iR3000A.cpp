/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// recompiler reworked to add dynamic linking Jan06
// and added reg caching, const propagation, block analysis Jun06
// zerofrog(@gmail.com)


#include "iR3000A.h"
#include "R3000A.h"
#include "BaseblockEx.h"
#include "System/RecTypes.h"
#include "IopBios.h"
#include "IopHw.h"
#include "Common.h"

#include <time.h>

#ifndef _WIN32
#include <sys/types.h>
#endif

#include "iCore.h"

#include "AppConfig.h"

using namespace x86Emitter;

extern void psxBREAK();

u32 g_psxMaxRecMem = 0;
u32 s_psxrecblocks[] = {0};

uptr psxRecLUT[0x10000];
u32 psxhwLUT[0x10000];

static __fi u32 HWADDR(u32 mem) { return psxhwLUT[mem >> 16] + mem; }

static RecompiledCodeReserve* recMem = NULL;

static BASEBLOCK *recRAM = NULL;	// and the ptr to the blocks here
static BASEBLOCK *recROM = NULL;	// and here
static BASEBLOCK *recROM1 = NULL;	// also here
static BASEBLOCK *recROM2 = NULL;   // also here
static BaseBlocks recBlocks;
static u8 *recPtr = NULL;
u32 psxpc;			// recompiler psxpc
int psxbranch;		// set for branch
u32 g_iopCyclePenalty;

static EEINST* s_pInstCache = NULL;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = NULL;
static BASEBLOCKEX* s_pCurBlockEx = NULL;

static u32 s_nEndBlock = 0; // what psxpc the current block ends
static u32 s_branchTo;
static bool s_nBlockFF;

static u32 s_saveConstRegs[32];
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0;
static EEINST* s_psaveInstInfo = NULL;

static u32 s_psxBlockCycles = 0; // cycles of current block recompiling
static u32 s_savenBlockCycles = 0;

static void iPsxBranchTest(u32 newpc, u32 cpuBranch);
void psxRecompileNextInstruction(int delayslot);

extern void (*rpsxBSC[64])();
void rpsxpropBSC(EEINST* prev, EEINST* pinst);

static void iopClearRecLUT(BASEBLOCK* base, int count);

#define PSX_GETBLOCK(x) PC_GETBLOCK_(x, psxRecLUT)

#define PSXREC_CLEARM(mem) \
	(((mem) < g_psxMaxRecMem && (psxRecLUT[(mem) >> 16] + (mem))) ? \
		psxRecClearMem(mem) : 4)

// =====================================================================================================
//  Dynamically Compiled Dispatchers - R3000A style
// =====================================================================================================

static void iopRecRecompile( const u32 startpc );

// Recompiled code buffer for EE recompiler dispatchers!
static u8 __pagealigned iopRecDispatchers[PCSX2_PAGESIZE];

typedef void DynGenFunc(void);

static DynGenFunc* iopDispatcherEvent		= NULL;
static DynGenFunc* iopDispatcherReg		= NULL;
static DynGenFunc* iopJITCompile		= NULL;
static DynGenFunc* iopJITCompileInBlock		= NULL;
static DynGenFunc* iopEnterRecompiledCode	= NULL;
static DynGenFunc* iopExitRecompiledCode	= NULL;

static void recEventTest(void)
{
	_cpuEventTest_Shared();
}

// The address for all cleared blocks.  It recompiles the current pc and then
// dispatches to the recompiled block address.
static DynGenFunc* _DynGen_JITCompile(void)
{
	u8* retval = xGetPtr();

	xFastCall((void*)iopRecRecompile, ptr32[&psxRegs.pc] );

	xMOV( eax, ptr[&psxRegs.pc] );
	xMOV( ebx, eax );
	xSHR( eax, 16 );
	xMOV( rcx, ptrNative[xComplexAddress(rcx, psxRecLUT, rax*wordsize)] );
	xJMP( ptrNative[rbx*(wordsize/4) + rcx] );

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_JITCompileInBlock(void)
{
	u8* retval = xGetPtr();
	xJMP( (void*)iopJITCompile );
	return (DynGenFunc*)retval;
}

// called when jumping to variable pc address
static DynGenFunc* _DynGen_DispatcherReg(void)
{
	u8* retval = xGetPtr();

	xMOV( eax, ptr[&psxRegs.pc] );
	xMOV( ebx, eax );
	xSHR( eax, 16 );
	xMOV( rcx, ptrNative[xComplexAddress(rcx, psxRecLUT, rax*wordsize)] );
	xJMP( ptrNative[rbx*(wordsize/4) + rcx] );

	return (DynGenFunc*)retval;
}

// --------------------------------------------------------------------------------------
//  EnterRecompiledCode  - dynamic compilation stub!
// --------------------------------------------------------------------------------------
static DynGenFunc* _DynGen_EnterRecompiledCode(void)
{
	// Optimization: The IOP never uses stack-based parameter invocation, so we can avoid
	// allocating any room on the stack for it (which is important since the IOP's entry
	// code gets invoked quite a lot).

	u8* retval = xGetPtr();

	{
		// Properly scope the frame prologue/epilogue
		xScopedStackFrame frame(false);

		xJMP((void*)iopDispatcherReg);

		// Save an exit point
		iopExitRecompiledCode = (DynGenFunc*)xGetPtr();
	}

	xRET();

	return (DynGenFunc*)retval;
}

static void _DynGen_Dispatchers(void)
{
	// In case init gets called multiple times:
	HostSys::MemProtectStatic( iopRecDispatchers, PageAccess_ReadWrite() );

	// clear the buffer to 0xcc (easier debugging).
	memset( iopRecDispatchers, 0xcc, PCSX2_PAGESIZE);

	xSetPtr( iopRecDispatchers );

	// Place the EventTest and DispatcherReg stuff at the top, because they get called the
	// most and stand to benefit from strong alignment and direct referencing.
	iopDispatcherEvent = (DynGenFunc*)xGetPtr();
	xFastCall((void*)recEventTest );
	iopDispatcherReg	= _DynGen_DispatcherReg();

	iopJITCompile			= _DynGen_JITCompile();
	iopJITCompileInBlock	= _DynGen_JITCompileInBlock();
	iopEnterRecompiledCode	= _DynGen_EnterRecompiledCode();

	HostSys::MemProtectStatic( iopRecDispatchers, PageAccess_ExecOnly() );

	recBlocks.SetJITCompile( iopJITCompile );
}

////////////////////////////////////////////////////
using namespace R3000A;

u8 _psxLoadWritesRs(u32 tempcode)
{
	switch(tempcode>>26) {
		case 32: case 33: case 34: case 35: case 36: case 37: case 38:
			return ((tempcode>>21)&0x1f)==((tempcode>>16)&0x1f); // rs==rt
	}
	return 0;
}

u8 _psxIsLoadStore(u32 tempcode)
{
	switch(tempcode>>26) {
		case 32: case 33: case 34: case 35: case 36: case 37: case 38:
		// 4 byte stores
		case 40: case 41: case 42: case 43: case 46:
			return 1;
	}
	return 0;
}

void _psxFlushAllUnused(void)
{
	int i;
	for(i = 0; i < 34; ++i) {
		if( psxpc < s_nEndBlock ) {
			if( (g_pCurInstInfo[1].regs[i]&EEINST_USED) )
				continue;
		}
		else if( (g_pCurInstInfo[0].regs[i]&EEINST_USED) )
			continue;

		if( i < 32 && PSX_IS_CONST1(i) ) _psxFlushConstReg(i);
		else {
			_deleteX86reg(X86TYPE_PSX, i, 1);
		}
	}
}

int _psxFlushUnusedConstReg()
{
	int i;
	for(i = 1; i < 32; ++i) {
		if( (g_psxHasConstReg & (1<<i)) && !(g_psxFlushedConstReg&(1<<i)) &&
			!_recIsRegWritten(g_pCurInstInfo+1, (s_nEndBlock-psxpc)/4, XMMTYPE_GPRREG, i) ) {

			// check if will be written in the future
			xMOV(ptr32[&psxRegs.GPR.r[i]], g_psxConstRegs[i]);
			g_psxFlushedConstReg |= 1<<i;
			return 1;
		}
	}

	return 0;
}

void _psxFlushCachedRegs()
{
	_psxFlushConstRegs();
}

void _psxFlushConstReg(int reg)
{
	if( PSX_IS_CONST1( reg ) && !(g_psxFlushedConstReg&(1<<reg)) ) {
		xMOV(ptr32[&psxRegs.GPR.r[reg]], g_psxConstRegs[reg]);
		g_psxFlushedConstReg |= (1<<reg);
	}
}

void _psxFlushConstRegs(void)
{
	int i;

	// flush constants

	// ignore r0
	for(i = 1; i < 32; ++i) {
		if( g_psxHasConstReg & (1<<i) ) {

			if( !(g_psxFlushedConstReg&(1<<i)) ) {
				xMOV(ptr32[&psxRegs.GPR.r[i]], g_psxConstRegs[i]);
				g_psxFlushedConstReg |= 1<<i;
			}

			if( g_psxHasConstReg == g_psxFlushedConstReg )
				break;
		}
	}
}

void _psxDeleteReg(int reg, int flush)
{
	if( !reg ) return;
	if( flush && PSX_IS_CONST1(reg) ) {
		_psxFlushConstReg(reg);
		return;
	}
	PSX_DEL_CONST(reg);
	_deleteX86reg(X86TYPE_PSX, reg, flush ? 0 : 2);
}

void _psxMoveGPRtoR(const xRegister32& to, int fromgpr)
{
	if( PSX_IS_CONST1(fromgpr) )
		xMOV(to, g_psxConstRegs[fromgpr] );
	else {
		// check x86
		xMOV(to, ptr[&psxRegs.GPR.r[ fromgpr ] ]);
	}
}

void _psxFlushCall(int flushtype)
{
	// x86-32 ABI : These registers are not preserved across calls:
	_freeX86reg( eax );
	_freeX86reg( ecx );
	_freeX86reg( edx );

	if( flushtype & FLUSH_CACHED_REGS )
		_psxFlushConstRegs();
}

void psxSaveBranchState(void)
{
	s_savenBlockCycles = s_psxBlockCycles;
	memcpy(s_saveConstRegs, g_psxConstRegs, sizeof(g_psxConstRegs));
	s_saveHasConstReg = g_psxHasConstReg;
	s_saveFlushedConstReg = g_psxFlushedConstReg;
	s_psaveInstInfo = g_pCurInstInfo;

	// save all regs
	memcpy(s_saveX86regs, x86regs, sizeof(x86regs));
}

void psxLoadBranchState(void)
{
	s_psxBlockCycles = s_savenBlockCycles;

	memcpy(g_psxConstRegs, s_saveConstRegs, sizeof(g_psxConstRegs));
	g_psxHasConstReg = s_saveHasConstReg;
	g_psxFlushedConstReg = s_saveFlushedConstReg;
	g_pCurInstInfo = s_psaveInstInfo;

	// restore all regs
	memcpy(x86regs, s_saveX86regs, sizeof(x86regs));
}

////////////////////
// Code Templates //
////////////////////

void _psxOnWriteReg(int reg)
{
	PSX_DEL_CONST(reg);
}

// rd = rs op rt
void psxRecompileCodeConst0(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode)
{
	if ( ! _Rd_ ) return;

	// for now, don't support xmm

	_deleteX86reg(X86TYPE_PSX, _Rs_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rt_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rd_, 0);

	if( PSX_IS_CONST2(_Rs_, _Rt_) ) {
		PSX_SET_CONST(_Rd_);
		constcode();
		return;
	}

	if( PSX_IS_CONST1(_Rs_) ) {
		constscode(0);
		PSX_DEL_CONST(_Rd_);
		return;
	}

	if( PSX_IS_CONST1(_Rt_) ) {
		consttcode(0);
		PSX_DEL_CONST(_Rd_);
		return;
	}

	noconstcode(0);
	PSX_DEL_CONST(_Rd_);
}

static void psxRecompileIrxImport(void)
{
	u32 import_table = irxImportTableAddr(psxpc - 4);
	u16 index = psxRegs.code & 0xffff;
	if (!import_table)
		return;

	const std::string libname = iopMemReadString(import_table + 12, 8);

	irxHLE hle = irxImportHLE(libname, index);
	const irxDEBUG debug = 0;

	if (!hle && !debug)
		return;

	xMOV(ptr32[&psxRegs.code], psxRegs.code);
	xMOV(ptr32[&psxRegs.pc], psxpc);
	_psxFlushCall(FLUSH_NODESTROY);

	if (debug)
		xFastCall((void *)debug);

	if (hle) {
		xFastCall((void *)hle);
		xTEST(eax, eax);
		xJNZ(iopDispatcherReg);
	}
}

// rt = rs op imm16
void psxRecompileCodeConst1(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode)
{
    if ( ! _Rt_ ) {
		// check for iop module import table magic
		if (psxRegs.code >> 16 == 0x2400)
			psxRecompileIrxImport();
        return;
    }

	// for now, don't support xmm

	_deleteX86reg(X86TYPE_PSX, _Rs_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rt_, 0);

	if( PSX_IS_CONST1(_Rs_) ) {
		PSX_SET_CONST(_Rt_);
		constcode();
		return;
	}

	noconstcode(0);
	PSX_DEL_CONST(_Rt_);
}

// rd = rt op sa
void psxRecompileCodeConst2(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode)
{
	if ( ! _Rd_ ) return;

	// for now, don't support xmm

	_deleteX86reg(X86TYPE_PSX, _Rt_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rd_, 0);

	if( PSX_IS_CONST1(_Rt_) ) {
		PSX_SET_CONST(_Rd_);
		constcode();
		return;
	}

	noconstcode(0);
	PSX_DEL_CONST(_Rd_);
}

// rd = rt MULT rs  (SPECIAL)
void psxRecompileCodeConst3(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode, int LOHI)
{
	_deleteX86reg(X86TYPE_PSX, _Rs_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rt_, 1);

	if( LOHI ) {
		_deleteX86reg(X86TYPE_PSX, PSX_HI, 1);
		_deleteX86reg(X86TYPE_PSX, PSX_LO, 1);
	}

	if( PSX_IS_CONST2(_Rs_, _Rt_) ) {
		constcode();
		return;
	}

	if( PSX_IS_CONST1(_Rs_) ) {
		constscode(0);
		return;
	}

	if( PSX_IS_CONST1(_Rt_) ) {
		consttcode(0);
		return;
	}

	noconstcode(0);
}

static uptr m_ConfiguredCacheReserve = 32;
static u8* m_recBlockAlloc = NULL;

static const uint m_recBlockAllocSize =
	(((Ps2MemSize::IopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4) * sizeof(BASEBLOCK));

static void recReserveCache(void)
{
	if (!recMem) recMem = new RecompiledCodeReserve(_8mb);

	while (!recMem->GetPtr())
	{
		if (recMem->Reserve(GetVmMemory().MainMemory(), HostMemoryMap::IOPrecOffset, m_ConfiguredCacheReserve * _1mb) != NULL) break;

		// If it failed, then try again (if possible):
		if (m_ConfiguredCacheReserve < 4) break;
		m_ConfiguredCacheReserve /= 2;
	}
}

static void recReserve(void)
{
	// IOP has no hardware requirements!
	recReserveCache();
}

static void recAlloc(void)
{
	// Goal: Allocate BASEBLOCKs for every possible branch target in IOP memory.
	// Any 4-byte aligned address makes a valid branch target as per MIPS design (all instructions are
	// always 4 bytes long).

	if( m_recBlockAlloc == NULL )
		m_recBlockAlloc = (u8*)AlignedMalloc( m_recBlockAllocSize, 4096 );
	u8* curpos = m_recBlockAlloc;
	recRAM = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::IopRam / 4) * sizeof(BASEBLOCK);
	recROM = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Rom / 4) * sizeof(BASEBLOCK);
	recROM1 = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Rom1 / 4) * sizeof(BASEBLOCK);
	recROM2 = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Rom2 / 4) * sizeof(BASEBLOCK);


	if( s_pInstCache == NULL )
	{
		s_nInstCacheSize = 128;
		s_pInstCache = (EEINST*)malloc( sizeof(EEINST) * s_nInstCacheSize );
	}

	_DynGen_Dispatchers();
}

void recResetIOP(void)
{
	recAlloc();
	recMem->Reset();

	iopClearRecLUT((BASEBLOCK*)m_recBlockAlloc,
		(((Ps2MemSize::IopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4)));

	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(psxRecLUT, 0, 0, 0, i, 0);

	// IOP knows 64k pages, hence for the 0x10000's

	// The bottom 2 bits of PC are always zero, so we <<14 to "compress"
	// the pc indexer into it's lower common denominator.

	// We're only mapping 20 pages here in 4 places.
	// 0x80 comes from : (Ps2MemSize::IopRam / 0x10000) * 4

	for (int i=0; i<0x80; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0x0000, i, i & 0x1f);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0x8000, i, i & 0x1f);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0xa000, i, i & 0x1f);
	}

	for (int i=0x1fc0; i<0x2000; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0xa000, i, i - 0x1fc0);
	}

	for (int i=0x1e00; i<0x1e40; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0xa000, i, i - 0x1e00);
	}

	for (int i = 0x1e40; i < 0x1e48; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0xa000, i, i - 0x1e40);
	}

	if( s_pInstCache )
		memset( s_pInstCache, 0, sizeof(EEINST)*s_nInstCacheSize );

	recBlocks.Reset();
	g_psxMaxRecMem = 0;

	recPtr = *recMem;
	psxbranch = 0;
}

static void recShutdown(void)
{
	safe_delete( recMem );

	safe_aligned_free( m_recBlockAlloc );

	safe_free( s_pInstCache );
	s_nInstCacheSize = 0;
}

static void iopClearRecLUT(BASEBLOCK* base, int count)
{
	for (int i = 0; i < count; i++)
		base[i].m_pFnptr = (uptr)iopJITCompile;
}

static void recExecute(void)
{
}

static __noinline s32 recExecuteBlock( s32 eeCycles )
{
	psxRegs.iopBreak   = 0;
	psxRegs.iopCycleEE = eeCycles;

	// [TODO] recExecuteBlock could be replaced by a direct call to the iopEnterRecompiledCode()
	//   (by assigning its address to the psxRec structure).  But for that to happen, we need
	//   to move iopBreak/iopCycleEE update code to emitted assembly code. >_<  --air

	// Likely Disasm, as borrowed from MSVC:

// Entry:
// 	mov         eax,dword ptr [esp+4]
// 	mov         dword ptr [psxRegs.iopBreak (0E88DCCh)],0
// 	mov         dword ptr [psxRegs.iopCycleEE (832A84h)],eax

// Exit:
// 	mov         ecx,dword ptr [psxRegs.iopBreak (0E88DCCh)]
// 	mov         edx,dword ptr [psxRegs.iopCycleEE (832A84h)]
// 	lea         eax,[edx+ecx]

	iopEnterRecompiledCode();

	return psxRegs.iopBreak + psxRegs.iopCycleEE;
}

// Returns the offset to the next instruction after any cleared memory
static __fi u32 psxRecClearMem(u32 pc)
{
	BASEBLOCK* pblock;

	pblock = PSX_GETBLOCK(pc);
	if (pblock->m_pFnptr == (uptr)iopJITCompile)
		return 4;

	pc = HWADDR(pc);

	u32 lowerextent = pc, upperextent = pc + 4;
	int blockidx = recBlocks.Index(pc);

	while (BASEBLOCKEX* pexblock = recBlocks[blockidx - 1]) {
		if (pexblock->startpc + pexblock->size * 4 <= lowerextent)
			break;

		lowerextent = std::min(lowerextent, pexblock->startpc);
		blockidx--;
	}

	int toRemoveFirst = blockidx;

	while (BASEBLOCKEX* pexblock = recBlocks[blockidx]) {
		if (pexblock->startpc >= upperextent)
			break;

		lowerextent = std::min(lowerextent, pexblock->startpc);
		upperextent = std::max(upperextent, pexblock->startpc + pexblock->size * 4);

		blockidx++;
	}

	if(toRemoveFirst != blockidx) {
		recBlocks.Remove(toRemoveFirst, (blockidx - 1));
	}

	blockidx=0;

	iopClearRecLUT(PSX_GETBLOCK(lowerextent), (upperextent - lowerextent) / 4);

	return upperextent - pc;
}

static __fi void recClearIOP(u32 Addr, u32 Size)
{
	u32 pc = Addr;
	while (pc < Addr + Size*4)
		pc += PSXREC_CLEARM(pc);
}

void psxSetBranchReg(u32 reg)
{
	psxbranch = 1;

	if( reg != 0xffffffff ) {
		_allocX86reg(calleeSavedReg2d, X86TYPE_PCWRITEBACK, 0, MODE_WRITE);
		_psxMoveGPRtoR(calleeSavedReg2d, reg);

		psxRecompileNextInstruction(1);

		if( x86regs[calleeSavedReg2d.GetId()].inuse )
		{
			xMOV(ptr32[&psxRegs.pc], calleeSavedReg2d);
			x86regs[calleeSavedReg2d.GetId()].inuse = 0;
		}
		else
		{
			xMOV(eax, ptr32[&psxRegs.pcWriteback]);
			xMOV(ptr32[&psxRegs.pc], eax);
		}
	}

	_psxFlushCall(FLUSH_EVERYTHING);
	iPsxBranchTest(0xffffffff, 1);

	JMP32((uptr)iopDispatcherReg - ( (uptr)x86Ptr + 5 ));
}

void psxSetBranchImm( u32 imm )
{
	psxbranch = 1;

	// end the current block
	xMOV(ptr32[&psxRegs.pc], imm );
	_psxFlushCall(FLUSH_EVERYTHING);
	iPsxBranchTest(imm, imm <= psxpc);

	recBlocks.Link(HWADDR(imm), xJcc32());
}

static __fi u32 psxScaleBlockCycles()
{
	return s_psxBlockCycles;
}

static void iPsxBranchTest(u32 newpc, u32 cpuBranch)
{
	u32 blockCycles = psxScaleBlockCycles();

	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && newpc == s_branchTo)
	{
		xMOV(eax, ptr32[&psxRegs.cycle]);
		xMOV(ecx, eax);
		xMOV(edx, ptr32[&psxRegs.iopCycleEE]);
		xADD(edx, 7);
		xSHR(edx, 3);
		xADD(eax, edx);
		xCMP(eax, ptr32[&psxRegs.iopNextEventCycle]);
		xCMOVNS(eax, ptr32[&psxRegs.iopNextEventCycle]);
		xMOV(ptr32[&psxRegs.cycle], eax);
		xSUB(eax, ecx);
		xSHL(eax, 3);
		xSUB(ptr32[&psxRegs.iopCycleEE], eax);
		xJLE(iopExitRecompiledCode);

		xFastCall((void*)iopEventTest);

		if( newpc != 0xffffffff )
		{
			xCMP(ptr32[&psxRegs.pc], newpc);
			xJNE(iopDispatcherReg);
		}
	}
	else
	{
		xMOV(eax, ptr32[&psxRegs.cycle]);
		xADD(eax, blockCycles);
		xMOV(ptr32[&psxRegs.cycle], eax); // update cycles

		// jump if psxRegs.iopCycleEE <= 0  (iop's timeslice timed out, so time to return control to the EE)
		xSUB(ptr32[&psxRegs.iopCycleEE], blockCycles*8);
		xJLE(iopExitRecompiledCode);

		// check if an event is pending
		xSUB(eax, ptr32[&psxRegs.iopNextEventCycle]);
		xForwardJS<u8> nointerruptpending;

		xFastCall((void*)iopEventTest);

		if( newpc != 0xffffffff ) {
			xCMP(ptr32[&psxRegs.pc], newpc);
			xJNE(iopDispatcherReg);
		}

		nointerruptpending.SetTarget();
	}
}

void rpsxSYSCALL(void)
{
	xMOV(ptr32[&psxRegs.code], psxRegs.code );
	xMOV(ptr32[&psxRegs.pc], psxpc - 4);
	_psxFlushCall(FLUSH_NODESTROY);

	//xMOV( ecx, 0x20 );			// exception code
	//xMOV( edx, psxbranch==1 );	// branch delay slot?
	xFastCall((void*)psxException, 0x20, psxbranch == 1 );

	xCMP(ptr32[&psxRegs.pc], psxpc-4);
	j8Ptr[0] = JE8(0);

	xADD(ptr32[&psxRegs.cycle], psxScaleBlockCycles() );
	xSUB(ptr32[&psxRegs.iopCycleEE], psxScaleBlockCycles()*8 );
	JMP32((uptr)iopDispatcherReg - ( (uptr)x86Ptr + 5 ));

	// jump target for skipping blockCycle updates
	x86SetJ8(j8Ptr[0]);

	//if (!psxbranch) psxbranch = 2;
}

void rpsxBREAK()
{
	xMOV(ptr32[&psxRegs.code], psxRegs.code );
	xMOV(ptr32[&psxRegs.pc], psxpc - 4);
	_psxFlushCall(FLUSH_NODESTROY);

	//xMOV( ecx, 0x24 );			// exception code
	//xMOV( edx, psxbranch==1 );	// branch delay slot?
	xFastCall((void*)psxException, 0x24, psxbranch == 1 );

	xCMP(ptr32[&psxRegs.pc], psxpc-4);
	j8Ptr[0] = JE8(0);
	xADD(ptr32[&psxRegs.cycle], psxScaleBlockCycles() );
	xSUB(ptr32[&psxRegs.iopCycleEE], psxScaleBlockCycles()*8 );
	JMP32((uptr)iopDispatcherReg - ( (uptr)x86Ptr + 5 ));
	x86SetJ8(j8Ptr[0]);

	//if (!psxbranch) psxbranch = 2;
}

void psxRecompileNextInstruction(int delayslot)
{
	// pblock isn't used elsewhere in this function.
	//BASEBLOCK* pblock = PSX_GETBLOCK(psxpc);

	psxRegs.code = iopMemRead32( psxpc );
	s_psxBlockCycles++;
	psxpc += 4;

	g_pCurInstInfo++;

	g_iopCyclePenalty = 0;
	rpsxBSC[ psxRegs.code >> 26 ]();
	s_psxBlockCycles += g_iopCyclePenalty;

	_clearNeededX86regs();
}

static void iopRecRecompile( const u32 startpc )
{
	u32 i;
	u32 willbranch3 = 0;

	// Inject IRX hack
	if (startpc == 0x1630 && g_Conf->CurrentIRX.Length() > 3) {
		if (iopMemRead32(0x20018) == 0x1F) {
			// FIXME do I need to increase the module count (0x1F -> 0x20)
			iopMemWrite32(0x20094, 0xbffc0000);
		}
	}

	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= (recMem->GetPtrEnd() - _64kb)) {
		recResetIOP();
	}

	x86SetPtr( recPtr );
	x86Align(16);
	recPtr = x86Ptr;

	s_pCurBlock = PSX_GETBLOCK(startpc);

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));

	if(!s_pCurBlockEx || s_pCurBlockEx->startpc != HWADDR(startpc))
		s_pCurBlockEx = recBlocks.New(HWADDR(startpc), (uptr)recPtr);

	psxbranch = 0;

	s_pCurBlock->m_pFnptr = (uptr)x86Ptr;
	s_psxBlockCycles = 0;

	// reset recomp state variables
	psxpc = startpc;
	g_psxHasConstReg = g_psxFlushedConstReg = 1;

	_initX86regs();

	if ((psxHu32(HW_ICFG) & 8) && (HWADDR(startpc) == 0xa0 || HWADDR(startpc) == 0xb0 || HWADDR(startpc) == 0xc0)) {
		xFastCall((void*)psxBiosCall);
		xTEST(al, al);
		xJNZ(iopDispatcherReg);
	}

	// go until the next branch
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;

	for(;;)
	{
		BASEBLOCK* pblock = PSX_GETBLOCK(i);
		if (i != startpc
		 && pblock->m_pFnptr != (uptr)iopJITCompile
		 && pblock->m_pFnptr != (uptr)iopJITCompileInBlock) {
			// branch = 3
			willbranch3 = 1;
			s_nEndBlock = i;
			break;
		}

		psxRegs.code = iopMemRead32(i);

		switch(psxRegs.code >> 26) {
			case 0: // special

				if( _Funct_ == 8 || _Funct_ == 9 ) { // JR, JALR
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}

				break;
			case 1: // regimm

				if( _Rt_ == 0 || _Rt_ == 1 || _Rt_ == 16 || _Rt_ == 17 ) {

					s_branchTo = _Imm_ * 4 + i + 4;
					if( s_branchTo > startpc && s_branchTo < i ) s_nEndBlock = s_branchTo;
					else  s_nEndBlock = i+8;

					goto StartRecomp;
				}

				break;

			case 2: // J
			case 3: // JAL
				s_branchTo = _InstrucTarget_ << 2 | (i + 4) & 0xf0000000;
				s_nEndBlock = i + 8;
				goto StartRecomp;

			// branches
			case 4: case 5: case 6: case 7:

				s_branchTo = _Imm_ * 4 + i + 4;
				if( s_branchTo > startpc && s_branchTo < i ) s_nEndBlock = s_branchTo;
				else  s_nEndBlock = i+8;

				goto StartRecomp;
		}

		i += 4;
	}

StartRecomp:

	s_nBlockFF = false;
	if (s_branchTo == startpc) {
		s_nBlockFF = true;
		for (i = startpc; i < s_nEndBlock; i += 4) {
			if (i != s_nEndBlock - 8) {
				switch (iopMemRead32(i)) {
					case 0: // nop
						break;
					default:
						s_nBlockFF = false;
				}
			}
		}
	}

	// rec info //
	{
		EEINST* pcur;

		if( s_nInstCacheSize < (s_nEndBlock-startpc)/4+1 ) {
			free(s_pInstCache);
			s_nInstCacheSize = (s_nEndBlock-startpc)/4+10;
			s_pInstCache = (EEINST*)malloc(sizeof(EEINST)*s_nInstCacheSize);
		}

		pcur = s_pInstCache + (s_nEndBlock-startpc)/4;
		_recClearInst(pcur);
		pcur->info = 0;

		for(i = s_nEndBlock; i > startpc; i -= 4 ) {
			psxRegs.code = iopMemRead32(i-4);
			pcur[-1] = pcur[0];
			rpsxpropBSC(pcur-1, pcur);
			pcur--;
		}
	}

	g_pCurInstInfo = s_pInstCache;
	while (!psxbranch && psxpc < s_nEndBlock) {
		psxRecompileNextInstruction(0);
	}

	s_pCurBlockEx->size = (psxpc-startpc)>>2;

	for(i = 1; i < (u32)s_pCurBlockEx->size; ++i) {
		if (s_pCurBlock[i].m_pFnptr == (uptr)iopJITCompile)
			s_pCurBlock[i].m_pFnptr = (uptr)iopJITCompileInBlock;
	}

	if( !(psxpc&0x10000000) )
		g_psxMaxRecMem = std::max( (psxpc&~0xa0000000), g_psxMaxRecMem );

	if( psxbranch == 2 ) {
		_psxFlushCall(FLUSH_EVERYTHING);

		iPsxBranchTest(0xffffffff, 1);

		JMP32((uptr)iopDispatcherReg - ( (uptr)x86Ptr + 5 ));
	}
	else {
		if( !psxbranch )
		{
			xADD(ptr32[&psxRegs.cycle], psxScaleBlockCycles() );
			xSUB(ptr32[&psxRegs.iopCycleEE], psxScaleBlockCycles()*8 );
		}

		if (willbranch3 || !psxbranch) {
			_psxFlushCall(FLUSH_EVERYTHING);
			xMOV(ptr32[&psxRegs.pc], psxpc);
			recBlocks.Link(HWADDR(s_nEndBlock), xJcc32() );
			psxbranch = 3;
		}
	}

	s_pCurBlockEx->x86size = xGetPtr() - recPtr;

	recPtr = xGetPtr();

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
}

static void recSetCacheReserve( uint reserveInMegs )
{
	m_ConfiguredCacheReserve = reserveInMegs;
}

static uint recGetCacheReserve()
{
	return m_ConfiguredCacheReserve;
}

R3000Acpu psxRec = {
	recReserve,
	recResetIOP,
	recExecute,
	recExecuteBlock,
	recClearIOP,
	recShutdown,
	
	recGetCacheReserve,
	recSetCacheReserve
};

