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

#include "Common.h"
#include "Memory.h"

#include "R5900Exceptions.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"
#include "BaseblockEx.h"
#include "System/RecTypes.h"

#include "vtlb.h"

#include "gui/SysThreads.h"
#include "Elfheader.h"

#include "Patch.h"

#include <utility>

#include "Utilities/FastJmp.h"

using namespace x86Emitter;
using namespace R5900;

#define PC_GETBLOCK(x) PC_GETBLOCK_(x, recLUT)

static u32 maxrecmem = 0;
static __aligned16 uptr recLUT[_64kb];
static __aligned16 u32 hwLUT[_64kb];

#define HWADDR(mem) (hwLUT[(mem) >> 16] + (mem))

static u32 s_nBlockCycles = 0; // cycles of current block recompiling

u32 pc;			         // recompiler pc
int g_branch;	         // set for branch

__aligned16 GPR_reg64 g_cpuConstRegs[32] = {0};
u32 g_cpuHasConstReg = 0, g_cpuFlushedConstReg = 0;
static bool g_cpuFlushedPC, g_cpuFlushedCode, g_maySignalException;
bool g_recompilingDelaySlot;

////////////////////////////////////////////////////////////////
// Static Private Variables - R5900 Dynarec

#define X86
#define RECCONSTBUF_SIZE 32768 // 64 bit consts in 32 bit units

static RecompiledCodeReserve* recMem = NULL;
static u8* recRAMCopy                = NULL;
static u8* recLutReserve_RAM         = NULL;
static const size_t recLutSize       = (Ps2MemSize::MainRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) * wordsize / 4;

static uptr m_ConfiguredCacheReserve = 64;

static u32* recConstBuf              = NULL;	// 64-bit pseudo-immediates
static BASEBLOCK *recRAM 	     = NULL;	// and the ptr to the blocks here
static BASEBLOCK *recROM 	     = NULL;	// and here
static BASEBLOCK *recROM1 	     = NULL;	// also here
static BASEBLOCK *recROM2 	     = NULL;    // also here

static BaseBlocks recBlocks;
static u8* recPtr 		     = NULL;
static u32 *recConstBufPtr 	     = NULL;
EEINST* s_pInstCache		     = NULL;
static u32 s_nInstCacheSize	     = 0;

static BASEBLOCK* s_pCurBlock	     = NULL;
static BASEBLOCKEX* s_pCurBlockEx    = NULL;
u32 s_nEndBlock			     = 0; // what pc the current block ends
u32 s_branchTo;
static bool s_nBlockFF;

// save states for branches
GPR_reg64 s_saveConstRegs[32];
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0;
static EEINST* s_psaveInstInfo = NULL;

static u32 s_savenBlockCycles = 0;

static __aligned16 u16 manual_page[Ps2MemSize::MainRam >> 12];
static __aligned16 u8 manual_counter[Ps2MemSize::MainRam >> 12];

static std::atomic<bool> eeRecIsReset(false);
static std::atomic<bool> eeRecNeedsReset(false);
static bool eeCpuExecuting = false;
static bool g_resetEeScalingStats = false;
static int g_patchesNeedRedo = 0;

/* Forward declarations */
// defined at AppCoreThread.cpp but unclean and should not be public. We're the only
// consumers of it, so it's declared only here.
void LoadAllPatchesAndStuff(const Pcsx2Config&);
static void recRecompile( const u32 startpc );
static void recClear(u32 addr, u32 size);

void _eeFlushAllUnused(void)
{
	u32 i;
	for(i = 0; i < 34; ++i)
	{
		if( pc < s_nEndBlock )
		{
			if( (g_pCurInstInfo[1].regs[i]&EEINST_USED) )
				continue;
		}
		else if( (g_pCurInstInfo[0].regs[i]&EEINST_USED) )
			continue;

		if( i < 32 && GPR_IS_CONST1(i) )
			_flushConstReg(i);
		else
			_deleteGPRtoXMMreg(i, 1);
	}

	//TODO when used info is done for FPU and VU0
	for(i = 0; i < iREGCNT_XMM; ++i) {
		if( xmmregs[i].inuse && xmmregs[i].type != XMMTYPE_GPRREG )
			_freeXMMreg(i);
	}
}

void _eeMoveGPRtoR(const xRegister32& to, int fromgpr)
{
	if( fromgpr == 0 )
		xXOR(to, to);	// zero register should use xor, thanks --air
	else if( GPR_IS_CONST1(fromgpr) )
		xMOV(to, g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;

		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 && (xmmregs[mmreg].mode&MODE_WRITE)) {
			xMOVD(to, xRegisterSSE(mmreg));
		}
		else {
			xMOV(to, ptr[&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] ]);
		}
	}
}

void _eeMoveGPRtoM(uptr to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		xMOV(ptr32[(u32*)(to)], g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;

		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 ) {
			xMOVSS(ptr[(void*)(to)], xRegisterSSE(mmreg));
		}
		else {
			xMOV(eax, ptr[&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] ]);
			xMOV(ptr[(void*)(to)], eax);
		}
	}
}

void eeSignExtendTo(int gpr, bool onlyupper)
{
	xCDQ();
	if (!onlyupper)
		xMOV(ptr32[&cpuRegs.GPR.r[gpr].UL[0]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[gpr].UL[1]], edx);
}

static int _flushXMMunused(void)
{
	u32 i;
	for (i=0; i<iREGCNT_XMM; i++)
	{
		if (!xmmregs[i].inuse || xmmregs[i].needed || !(xmmregs[i].mode&MODE_WRITE) )
			continue;

		if (xmmregs[i].type == XMMTYPE_GPRREG )
		{
			if( !_recIsRegWritten(g_pCurInstInfo+1, (s_nEndBlock-pc)/4, XMMTYPE_GPRREG, xmmregs[i].reg) )
			{
				_freeXMMreg(i);
				xmmregs[i].inuse = 1;
				return 1;
			}
		}
	}

	return 0;
}

// Some of the generated MMX code needs 64-bit immediates but x86 doesn't
// provide this.  One of the reasons we are probably better off not doing
// MMX register allocation for the EE.
u32* recGetImm64(u32 hi, u32 lo)
{
	static u32 *imm64_cache[509];
	int cacheidx = lo % (sizeof imm64_cache / sizeof *imm64_cache);
	u32 *imm64   = imm64_cache[cacheidx]; // returned pointer
	if (imm64 && imm64[0] == lo && imm64[1] == hi)
		return imm64;

	if (recConstBufPtr >= recConstBuf + RECCONSTBUF_SIZE)
		throw Exception::ExitCpuExecute();

	imm64 = recConstBufPtr;
	recConstBufPtr += 2;
	imm64_cache[cacheidx] = imm64;

	imm64[0] = lo;
	imm64[1] = hi;

	return imm64;
}

// Use this to call into interpreter functions that require an immediate branchtest
// to be done afterward (anything that throws an exception or enables interrupts, etc).
void recBranchCall( void (*func)(void) )
{
	// In order to make sure a branch test is performed, the nextBranchCycle is set
	// to the current cpu cycle.

	xMOV(eax, ptr[&cpuRegs.cycle ]);
	xMOV(ptr[&cpuRegs.nextEventCycle], eax);

	recCall(func);
	g_branch = 2;
}

void recCall( void (*func)(void) )
{
	iFlushCall(FLUSH_INTERPRETER);
	xFastCall((void*)func);
}

// =====================================================================================================
//  R5900 Dispatchers
// =====================================================================================================

typedef void DynGenFunc(void);

static DynGenFunc* DispatcherEvent		= NULL;
static DynGenFunc* DispatcherReg		= NULL;
static DynGenFunc* JITCompile			= NULL;
static DynGenFunc* JITCompileInBlock	= NULL;
static DynGenFunc* EnterRecompiledCode	= NULL;
static DynGenFunc* ExitRecompiledCode	= NULL;
static DynGenFunc* DispatchBlockDiscard = NULL;
static DynGenFunc* DispatchPageReset    = NULL;

// Note: scaleblockcycles() scales s_nBlockCycles respective to the EECycleRate value for manipulating the cycles of current block recompiling.
// s_nBlockCycles is 3 bit fixed point.  Divide by 8 when done!
// Scaling blocks under 40 cycles seems to produce countless problem, so let's try to avoid them.

#define DEFAULT_SCALED_BLOCKS() (s_nBlockCycles >> 3)

static u32 scaleblockcycles_calculation(void)
{
	bool lowcycles   = (s_nBlockCycles <= 40);
	s8 cyclerate     = EmuConfig.Speedhacks.EECycleRate;
	u32 scale_cycles = 0;

	if (cyclerate == 0 || lowcycles || cyclerate < -99 || cyclerate > 3)
		scale_cycles = DEFAULT_SCALED_BLOCKS();

	else if (cyclerate > 1)
		scale_cycles = s_nBlockCycles >> (2 + cyclerate);

	else if (cyclerate == 1)
		scale_cycles = DEFAULT_SCALED_BLOCKS() / 1.3f; // Adds a mild 30% increase in clockspeed for value 1.

	else if (cyclerate == -1)  // the mildest value which is also used by the "balanced" preset.
		// These values were manually tuned to yield mild speedup with high compatibility
		scale_cycles = (s_nBlockCycles <= 80 || s_nBlockCycles > 168 ? 5 : 7) * s_nBlockCycles / 32;

	else
		scale_cycles = ((5 + (-2 * (cyclerate + 1))) * s_nBlockCycles) >> 5;

	// Ensure block cycle count is never less than 1.
	return (scale_cycles < 1) ? 1 : scale_cycles;
}


// called when jumping to variable pc address
static DynGenFunc* _DynGen_DispatcherReg(void)
{
	u8* retval = xGetPtr();		// fallthrough target, can't align it!

	// C equivalent:
	// u32 addr = cpuRegs.pc;
	// void(**base)() = (void(**)())recLUT[addr >> 16];
	// base[addr >> 2]();
	xMOV( eax, ptr[&cpuRegs.pc] );
	xMOV( ebx, eax );
	xSHR( eax, 16 );
	xMOV( rcx, ptrNative[xComplexAddress(rcx, recLUT, rax*wordsize)] );
	xJMP( ptrNative[rbx*(wordsize/4) + rcx] );

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_DispatcherEvent(void)
{
	u8* retval = xGetPtr();

	xFastCall((void*)_cpuEventTest_Shared );

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_EnterRecompiledCode(void)
{
	u8* retval = xGetAlignedCallTarget();

	{
		// Properly scope the frame prologue/epilogue
		xScopedStackFrame frame(false);

		xJMP((void*)DispatcherReg);

		// Save an exit point
		ExitRecompiledCode = (DynGenFunc*)xGetPtr();
	}

	xRET();

	return (DynGenFunc*)retval;
}

// The address for all cleared blocks.  It recompiles the current pc and then
// dispatches to the recompiled block address.
static DynGenFunc* _DynGen_JITCompile(void)
{
	u8* retval = xGetAlignedCallTarget();

	xFastCall((void*)recRecompile, ptr32[&cpuRegs.pc] );

	// C equivalent:
	// u32 addr = cpuRegs.pc;
	// void(**base)() = (void(**)())recLUT[addr >> 16];
	// base[addr >> 2]();
	xMOV( eax, ptr[&cpuRegs.pc] );
	xMOV( ebx, eax );
	xSHR( eax, 16 );
	xMOV( rcx, ptrNative[xComplexAddress(rcx, recLUT, rax*wordsize)] );
	xJMP( ptrNative[rbx*(wordsize/4) + rcx] );

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_JITCompileInBlock(void)
{
	u8* retval = xGetAlignedCallTarget();
	xJMP( (void*)JITCompile );
	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_DispatchBlockDiscard(void)
{
	u8* retval = xGetPtr();
	xFastCall((void*)recClear);
	xJMP((void*)ExitRecompiledCode);
	return (DynGenFunc*)retval;
}

// called when a page under manual protection has been run enough times to be a candidate
// for being reset under the faster vtlb write protection.  All blocks in the page are cleared
// and the block is re-assigned for write protection.
static void dyna_page_reset(u32 start,u32 sz)
{
	recClear(start & ~0xfffUL, 0x400);
	manual_counter[start >> 12]++;
	mmap_MarkCountedRamPage( start );
}

static DynGenFunc* _DynGen_DispatchPageReset()
{
	u8* retval = xGetPtr();
	xFastCall((void*)dyna_page_reset);
	xJMP((void*)ExitRecompiledCode);
	return (DynGenFunc*)retval;
}

static void _DynGen_Dispatchers(void)
{
	// Recompiled code buffer for EE recompiler dispatchers!
	static u8 __pagealigned eeRecDispatchers[PCSX2_PAGESIZE];
	// In case init gets called multiple times:
	HostSys::MemProtectStatic( eeRecDispatchers, PageAccess_ReadWrite() );

	// clear the buffer to 0xcc (easier debugging).
	memset( eeRecDispatchers, 0xcc, PCSX2_PAGESIZE);

	xSetPtr( eeRecDispatchers );

	// Place the EventTest and DispatcherReg stuff at the top, because they get called the
	// most and stand to benefit from strong alignment and direct referencing.
	DispatcherEvent = _DynGen_DispatcherEvent();
	DispatcherReg	= _DynGen_DispatcherReg();

	JITCompile           = _DynGen_JITCompile();
	JITCompileInBlock    = _DynGen_JITCompileInBlock();
	EnterRecompiledCode  = _DynGen_EnterRecompiledCode();
	DispatchBlockDiscard = _DynGen_DispatchBlockDiscard();
	DispatchPageReset    = _DynGen_DispatchPageReset();

	HostSys::MemProtectStatic( eeRecDispatchers, PageAccess_ExecOnly() );

	recBlocks.SetJITCompile( JITCompile );
}

static void recAlloc(void)
{
	if (!recRAMCopy)
		recRAMCopy = (u8*)AlignedMalloc(Ps2MemSize::MainRam, 4096);

	if (!recRAM)
		recLutReserve_RAM = (u8*)AlignedMalloc(recLutSize, 4096);

	BASEBLOCK* basepos = (BASEBLOCK*)recLutReserve_RAM;
	recRAM		= basepos; basepos += (Ps2MemSize::MainRam / 4);
	recROM		= basepos; basepos += (Ps2MemSize::Rom / 4);
	recROM1		= basepos; basepos += (Ps2MemSize::Rom1 / 4);
	recROM2		= basepos; basepos += (Ps2MemSize::Rom2 / 4);

	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(recLUT, 0, 0, 0, i, 0);

	for ( int i = 0x0000; i < (int)(Ps2MemSize::MainRam / 0x10000); i++ )
	{
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x0000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x2000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x3000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x8000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xa000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xb000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xc000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xd000, i, i);
	}

	for ( int i = 0x1fc0; i < 0x2000; i++ )
	{
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0xa000, i, i - 0x1fc0);
	}

	for ( int i = 0x1e00; i < 0x1e40; i++ )
	{
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0xa000, i, i - 0x1e00);
	}

	for (int i = 0x1e40; i < 0x1e48; i++) 
	{
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0xa000, i, i - 0x1e40);
	}

	if( !recConstBuf )
		recConstBuf = (u32*) AlignedMalloc( RECCONSTBUF_SIZE * sizeof(*recConstBuf), 16 );

	if( !s_pInstCache )
	{
		s_nInstCacheSize = 128;
		s_pInstCache = (EEINST*)malloc( sizeof(EEINST) * s_nInstCacheSize );
	}

	// No errors.. Proceed with initialization:
	_DynGen_Dispatchers();
}

static void recResetRaw(void)
{
	recAlloc();

	if( eeRecIsReset.exchange(true) ) return;
	eeRecNeedsReset = false;

	recMem->Reset();
	{
		BASEBLOCK *base = (BASEBLOCK*)recLutReserve_RAM;
		int memsize     = recLutSize;
		for (int i = 0; i < memsize/(int)sizeof(uptr); i++)
			base[i].m_pFnptr = (uptr)JITCompile;
	}
	memset(recRAMCopy, 0, Ps2MemSize::MainRam);

	maxrecmem = 0;

	memset(recConstBuf, 0, RECCONSTBUF_SIZE * sizeof(*recConstBuf));

	if( s_pInstCache )
		memset( s_pInstCache, 0, sizeof(EEINST)*s_nInstCacheSize );

	recBlocks.Reset();
	mmap_ResetBlockTracking();

	x86SetPtr(*recMem);

	recPtr = *recMem;
	recConstBufPtr = recConstBuf;

	g_branch = 0;
	g_resetEeScalingStats = true;
	g_patchesNeedRedo = 1;
}

static void memory_protect_recompiled_code(u32 startpc, u32 size)
{
	u32 inpage_ptr = HWADDR(startpc);
	u32 inpage_sz  = size*4;

	// The kernel context register is stored @ 0x800010C0-0x80001300
	// The EENULL thread context register is stored @ 0x81000-....
	bool contains_thread_stack = ((startpc >> 12) == 0x81) || ((startpc >> 12) == 0x80001);

	// note: blocks are guaranteed to reside within the confines of a single page.
	const vtlb_ProtectionMode PageType = contains_thread_stack ? ProtMode_Manual : mmap_GetRamPageInfo( inpage_ptr );

	switch (PageType)
	{
		case ProtMode_NotRequired:
			break;

		case ProtMode_None:
		case ProtMode_Write:
			mmap_MarkCountedRamPage( inpage_ptr );
			manual_page[inpage_ptr >> 12] = 0;
			break;

		case ProtMode_Manual:
			xMOV( arg1regd, inpage_ptr );
			xMOV( arg2regd, inpage_sz / 4 );

			u32 lpc = inpage_ptr;
			u32 stg = inpage_sz;

			while(stg>0)
			{
				xCMP( ptr32[PSM(lpc)], *(u32*)PSM(lpc) );
				xJNE(DispatchBlockDiscard);

				stg -= 4;
				lpc += 4;
			}

			// Tweakpoint!  3 is a 'magic' number representing the number of times a counted block
			// is re-protected before the recompiler gives up and sets it up as an uncounted (permanent)
			// manual block.  Higher thresholds result in more recompilations for blocks that share code
			// and data on the same page.  Side effects of a lower threshold: over extended gameplay
			// with several map changes, a game's overall performance could degrade.

			// (ideally, perhaps, manual_counter should be reset to 0 every few minutes?)

			if (!contains_thread_stack && manual_counter[inpage_ptr >> 12] <= 3)
			{
				// Counted blocks add a weighted (by block size) value into manual_page each time they're
				// run.  If the block gets run a lot, it resets and re-protects itself in the hope
				// that whatever forced it to be manually-checked before was a 1-time deal.

				// Counted blocks have a secondary threshold check in manual_counter, which forces a block
				// to 'uncounted' mode if it's recompiled several times.  This protects against excessive
				// recompilation of blocks that reside on the same codepage as data.

				// fixme? Currently this algo is kinda dumb and results in the forced recompilation of a
				// lot of blocks before it decides to mark a 'busy' page as uncounted.  There might be
				// be a more clever approach that could streamline this process, by doing a first-pass
				// test using the vtlb memory protection (without recompilation!) to reprotect a counted
				// block.  But unless a new algo is relatively simple in implementation, it's probably
				// not worth the effort (tests show that we have lots of recompiler memory to spare, and
				// that the current amount of recompilation is fairly cheap).

				xADD(ptr16[&manual_page[inpage_ptr >> 12]], size);
				xJC(DispatchPageReset);

				// note: clearcnt is measured per-page, not per-block!
			}
			break;
	}
}

// Generates dynarec code for Event tests followed by a block dispatch (branch).
// Parameters:
//   newpc - address to jump to at the end of the block.  If newpc == 0xffffffff then
//   the jump is assumed to be to a register (dynamic).  For any other value the
//   jump is assumed to be static, in which case the block will be "hardlinked" after
//   the first time it's dispatched.
//
//   noDispatch - When set true, then jump to Dispatcher.  Used by the recs
//   for blocks which perform exception checks without branching (it's enabled by
//   setting "g_branch = 2";
static void iBranchTest(u32 newpc)
{
	// Check the Event scheduler if our "cycle target" has been reached.
	// Equiv code to:
	//    cpuRegs.cycle += blockcycles;
	//    if( cpuRegs.cycle > cpuRegs.nextEventCycle ) { DoEvents(); }

	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && newpc == s_branchTo)
	{
		xMOV(eax, ptr32[&cpuRegs.nextEventCycle]);
		xADD(ptr32[&cpuRegs.cycle], scaleblockcycles_calculation());
		xCMP(eax, ptr32[&cpuRegs.cycle]);
		xCMOVS(eax, ptr32[&cpuRegs.cycle]);
		xMOV(ptr32[&cpuRegs.cycle], eax);

		xJMP( (void*)DispatcherEvent );
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.cycle]);
		xADD(eax, scaleblockcycles_calculation());
		xMOV(ptr[&cpuRegs.cycle], eax); // update cycles
		xSUB(eax, ptr[&cpuRegs.nextEventCycle]);

		if (newpc == 0xffffffff)
			xJS( DispatcherReg );
		else
			recBlocks.Link(HWADDR(newpc), xJcc32(Jcc_Signed));

		xJMP( (void*)DispatcherEvent );
	}
}


// Skip MPEG Game-Fix
static bool skipMPEG_By_Pattern(u32 sPC)
{
	if (!CHECK_SKIPMPEGHACK) return 0;

	// sceMpegIsEnd: lw reg, 0x40(a0); jr ra; lw v0, 0(reg)
	if ((s_nEndBlock == sPC + 12) && (memRead32(sPC + 4) == 0x03e00008)) {
		u32 code = memRead32(sPC);
		u32 p1   = 0x8c800040;
		u32 p2	 = 0x8c020000 | (code & 0x1f0000) << 5;
		if ((code & 0xffe0ffff)   != p1) return 0;
		if (memRead32(sPC+8) != p2) return 0;
		xMOV(ptr32[&cpuRegs.GPR.n.v0.UL[0]], 1);
		xMOV(ptr32[&cpuRegs.GPR.n.v0.UL[1]], 0);
		xMOV(eax, ptr32[&cpuRegs.GPR.n.ra.UL[0]]);
		xMOV(ptr32[&cpuRegs.pc], eax);
		iBranchTest(0xffffffff);
		g_branch = 1;
		pc = s_nEndBlock;
		return 1;
	}
	return 0;
}

static void doPlace0Patches(void)
{
    LoadAllPatchesAndStuff(EmuConfig);
    ApplyLoadedPatches(PPT_ONCE_ON_LOAD);
}

static void recRecompile( const u32 startpc )
{
	u32 i = 0;
	u32 willbranch3 = 0;
	u32 usecop2;

	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= (recMem->GetPtrEnd() - _64kb))
		eeRecNeedsReset = true;
	else if ((recConstBufPtr - recConstBuf) >= RECCONSTBUF_SIZE - 64)
		eeRecNeedsReset = true;

	if (eeRecNeedsReset) recResetRaw();

	xSetPtr( recPtr );
	recPtr = xGetAlignedCallTarget();

	s_pCurBlock = PC_GETBLOCK(startpc);

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));

	s_pCurBlockEx = recBlocks.New(HWADDR(startpc), (uptr)recPtr);

	if (HWADDR(startpc) == EELOAD_START)
	{
		// The EELOAD _start function is the same across all BIOS versions
		u32 mainjump = memRead32(EELOAD_START + 0x9c);
		if (mainjump >> 26 == 3) // JAL
			g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | (mainjump << 2 & 0x0fffffffU);
	}

	if (g_eeloadMain && HWADDR(startpc) == HWADDR(g_eeloadMain))
	{
		xFastCall((void*)eeloadHook);
		if (g_SkipBiosHack)
		{
			// There are four known versions of EELOAD, identifiable by the location of the 'jal' to the EELOAD function which
			// calls ExecPS2(). The function itself is at the same address in all BIOSs after v1.00-v1.10.
			u32 typeAexecjump = memRead32(EELOAD_START + 0x470); // v1.00, v1.01?, v1.10?
			u32 typeBexecjump = memRead32(EELOAD_START + 0x5B0); // v1.20, v1.50, v1.60 (3000x models)
			u32 typeCexecjump = memRead32(EELOAD_START + 0x618); // v1.60 (3900x models)
			u32 typeDexecjump = memRead32(EELOAD_START + 0x600); // v1.70, v1.90, v2.00, v2.20, v2.30
			if ((typeBexecjump >> 26 == 3) || (typeCexecjump >> 26 == 3) || (typeDexecjump >> 26 == 3)) // JAL to 0x822B8
				g_eeloadExec = EELOAD_START + 0x2B8;
			else if (typeAexecjump >> 26 == 3) // JAL to 0x82170
				g_eeloadExec = EELOAD_START + 0x170;
		}

		// On fast/full boot this will have a crc of 0x0. But when the game/elf itself is
		// recompiled (below - ElfEntry && g_GameLoading), then the crc would be from the elf.
		// g_patchesNeedRedo is set on rec reset, and this is the only consumer.
		// Also makes sure that patches from the previous elf/game are not applied on boot.
		if (g_patchesNeedRedo)
			doPlace0Patches();
		g_patchesNeedRedo = 0;
	}
	
	if (g_eeloadExec && HWADDR(startpc) == HWADDR(g_eeloadExec))
		xFastCall((void*)eeloadHook2);

	// this is the only way patches get applied, doesn't depend on a hack
	if (g_GameLoading && HWADDR(startpc) == ElfEntry)
	{
		xFastCall((void*)eeGameStarting);
		// Apply patch as soon as possible. Normally it is done in
		// eeGameStarting but first block is already compiled.
		doPlace0Patches();
	}

	g_branch         = 0;

	// reset recomp state variables
	s_nBlockCycles   = 0;
	pc               = startpc;
	g_cpuHasConstReg = g_cpuFlushedConstReg = 1;

	_initX86regs();
	_initXMMregs();

	if (EmuConfig.Gamefixes.GoemonTlbHack)
	{
		if (pc == 0x33ad48 || pc == 0x35060c)
		{
			// 0x33ad48 and 0x35060c are the return address of the function (0x356250) that populate the TLB cache
			xFastCall((void*)GoemonPreloadTlb);
		}
		else if (pc == 0x3563b8)
		{
			// Game will unmap some virtual addresses. If a constant address were hardcoded in the block, we would be in a bad situation.
			eeRecNeedsReset = true;
			// 0x3563b8 is the start address of the function that invalidate entry in TLB cache
			xFastCall((void*)GoemonUnloadTlb, ptr32[&cpuRegs.GPR.n.a0.UL[0]]);
		}
	}

	// go until the next branch
	i           = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo  = -1;

	for(;;)
	{
		BASEBLOCK* pblock = PC_GETBLOCK(i);

		if(i != startpc)	// Block size truncation checks.
		{
			if( (i & 0xffc) == 0x0 )	// breaks blocks at 4k page boundaries
			{
				willbranch3 = 1;
				s_nEndBlock = i;
				break;
			}

			if (pblock->m_pFnptr != (uptr)JITCompile && pblock->m_pFnptr != (uptr)JITCompileInBlock)
			{
				willbranch3 = 1;
				s_nEndBlock = i;
				break;
			}
		}

		//HUH ? PSM ? whut ? THIS IS VIRTUAL ACCESS GOD DAMMIT
		cpuRegs.code = *(int *)PSM(i);

		switch(cpuRegs.code >> 26) {
			case 0: // special
				if( _Funct_ == 8 || _Funct_ == 9 ) { // JR, JALR
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				break;

			case 1: // regimm

				if( _Rt_ < 4 || (_Rt_ >= 16 && _Rt_ < 20) ) {
					// branches
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
			case 20: case 21: case 22: case 23:
				s_branchTo = _Imm_ * 4 + i + 4;
				if( s_branchTo > startpc && s_branchTo < i ) s_nEndBlock = s_branchTo;
				else  s_nEndBlock = i+8;

				goto StartRecomp;

			case 16: // cp0
				if( _Rs_ == 16 ) {
					if( _Funct_ == 24 ) { // eret
						s_nEndBlock = i+4;
						goto StartRecomp;
					}
				}
				// Fall through!
				// COP0's branch opcodes line up with COP1 and COP2's

			case 17: // cp1
			case 18: // cp2
				if( _Rs_ == 8 ) {
					// BC1F, BC1T, BC1FL, BC1TL
					// BC2F, BC2T, BC2FL, BC2TL
					s_branchTo = _Imm_ * 4 + i + 4;
					if( s_branchTo > startpc && s_branchTo < i ) s_nEndBlock = s_branchTo;
					else  s_nEndBlock = i+8;

					goto StartRecomp;
				}
				break;
		}

		i += 4;
	}

StartRecomp:

	// The idea here is that as long as a loop doesn't write to a register it's already read
	// (excepting registers initialised with constants or memory loads) or use any instructions
	// which alter the machine state apart from registers, it will do the same thing on every
	// iteration.
	// TODO: special handling for counting loops.  God of war wastes time in a loop which just
	// counts to some large number and does nothing else, many other games use a counter as a
	// timeout on a register read.  AFAICS the only way to optimise this for non-const cases
	// without a significant loss in cycle accuracy is with a division, but games would probably
	// be happy with time wasting loops completing in 0 cycles and timeouts waiting forever.
	s_nBlockFF = false;
	if (s_branchTo == startpc) {
		s_nBlockFF = true;

		u32 reads = 0, loads = 1;

		for (i = startpc; i < s_nEndBlock; i += 4) {
			if (i == s_nEndBlock - 8)
				continue;
			cpuRegs.code = *(u32*)PSM(i);
			// nop
			if (cpuRegs.code == 0)
				continue;
			// cache, sync
			else if (_Opcode_ == 057 || _Opcode_ == 0 && _Funct_ == 017)
				continue;
			// imm arithmetic
			else if ((_Opcode_ & 070) == 010 || (_Opcode_ & 076) == 030)
			{
				if (loads & 1 << _Rs_) {
					loads |= 1 << _Rt_;
					continue;
				}
				else
					reads |= 1 << _Rs_;
				if (reads & 1 << _Rt_) {
					s_nBlockFF = false;
					break;
				}
			}
			// common register arithmetic instructions
			else if (_Opcode_ == 0 && (_Funct_ & 060) == 040 && (_Funct_ & 076) != 050)
			{
				if (loads & 1 << _Rs_ && loads & 1 << _Rt_) {
					loads |= 1 << _Rd_;
					continue;
				}
				else
					reads |= 1 << _Rs_ | 1 << _Rt_;
				if (reads & 1 << _Rd_) {
					s_nBlockFF = false;
					break;
				}
			}
			// loads
			else if ((_Opcode_ & 070) == 040 || (_Opcode_ & 076) == 032 || _Opcode_ == 067)
			{
				if (loads & 1 << _Rs_) {
					loads |= 1 << _Rt_;
					continue;
				}
				else
					reads |= 1 << _Rs_;
				if (reads & 1 << _Rt_) {
					s_nBlockFF = false;
					break;
				}
			}
			// mfc*, cfc*
			else if ((_Opcode_ & 074) == 020 && _Rs_ < 4)
			{
				loads |= 1 << _Rt_;
			}
			else
			{
				s_nBlockFF = false;
				break;
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
			cpuRegs.code = *(int *)PSM(i-4);
			pcur[-1] = pcur[0];
			pcur--;
		}
	}

	// analyze instructions //
	{
		usecop2 = 0;
		g_pCurInstInfo = s_pInstCache;

		for(i = startpc; i < s_nEndBlock; i += 4)
		{
			g_pCurInstInfo++;
			cpuRegs.code = *(u32*)PSM(i);

			// cop2 //
			if( g_pCurInstInfo->info & EEINSTINFO_COP2 ) {

				// init
				if(!usecop2)
					usecop2 = 1;

				VU0.code = cpuRegs.code;
				continue;
			}
		}
		// This *is* important because g_pCurInstInfo is checked a bit later on and
		// if it's not equal to s_pInstCache it handles recompilation differently.
		// ... but the empty if() conditional inside the for loop is still amusing. >_<
		if( usecop2 )
		{
			// add necessary mac writebacks
			g_pCurInstInfo = s_pInstCache;

			for(i = startpc; i < s_nEndBlock-4; i += 4)
				g_pCurInstInfo++;
		}
	}

	// Detect and handle self-modified code
	memory_protect_recompiled_code(startpc, (s_nEndBlock-startpc) >> 2);

	// Skip Recompilation if sceMpegIsEnd Pattern detected
	bool doRecompilation = !skipMPEG_By_Pattern(startpc);

	if (doRecompilation)
	{
		// Finally: Generate x86 recompiled code!
		g_pCurInstInfo = s_pInstCache;
		while (!g_branch && pc < s_nEndBlock)
			recompileNextInstruction(0);		// For the love of recursion, batman!
	}

	s_pCurBlockEx->size = (pc-startpc)>>2;

	if (HWADDR(pc) <= Ps2MemSize::MainRam) {
		BASEBLOCKEX *oldBlock;
		int i = recBlocks.LastIndex(HWADDR(pc) - 4);
		while (oldBlock = recBlocks[i--]) {
			if (oldBlock == s_pCurBlockEx)
				continue;
			if (oldBlock->startpc >= HWADDR(pc))
				continue;
			if ((oldBlock->startpc + oldBlock->size * 4) <= HWADDR(startpc))
				break;

			if (memcmp(&recRAMCopy[oldBlock->startpc / 4], PSM(oldBlock->startpc),
			           oldBlock->size * 4))
			{
				recClear(startpc, (pc - startpc) / 4);
				s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
				break;
			}
		}

		memcpy(&recRAMCopy[HWADDR(startpc) / 4], PSM(startpc), pc - startpc);
	}

	s_pCurBlock->m_pFnptr = (uptr)recPtr;

	for(i = 1; i < (u32)s_pCurBlockEx->size; i++) {
		if ((uptr)JITCompile == s_pCurBlock[i].m_pFnptr)
			s_pCurBlock[i].m_pFnptr = (uptr)JITCompileInBlock;
	}

	if( !(pc&0x10000000) )
		maxrecmem = std::max( (pc&~0xa0000000), maxrecmem );

	if( g_branch == 2 )
	{
		// Branch type 2 - This is how I "think" this works (air):
		// Performs a branch/event test but does not actually "break" the block.
		// This allows exceptions to be raised, and is thus sufficient for
		// certain types of things like SYSCALL, EI, etc.  but it is not sufficient
		// for actual branching instructions.

		iFlushCall(FLUSH_EVERYTHING);
		iBranchTest(0xffffffff);
	}
	else
	{
		if( willbranch3 || !g_branch) {

			iFlushCall(FLUSH_EVERYTHING);

			// Split Block concatenation mode.
			// This code is run when blocks are split either to keep block sizes manageable
			// or because we're crossing a 4k page protection boundary in ps2 mem.  The latter
			// case can result in very short blocks which should not issue branch tests for
			// performance reasons.

			int numinsts = (pc - startpc) / 4;
			if( numinsts > 6 )
				SetBranchImm(pc);
			else
			{
				xMOV( ptr32[&cpuRegs.pc], pc );
				xADD( ptr32[&cpuRegs.cycle], scaleblockcycles_calculation() );
				recBlocks.Link( HWADDR(pc), xJcc32() );
			}
		}
	}

	s_pCurBlockEx->x86size = xGetPtr() - recPtr;

	recPtr = xGetPtr();

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
}

// Size is in dwords (4 bytes)
static void recClear(u32 addr, u32 size)
{
	int blockidx;
	if ((addr) >= maxrecmem || !(recLUT[(addr) >> 16] + (addr & ~0xFFFFUL)))
		return;
	addr = HWADDR(addr);

	if ((blockidx = recBlocks.LastIndex(addr + size * 4 - 4)) == -1)
		return;

	u32 lowerextent = (u32)-1, upperextent = 0, ceiling = (u32)-1;

	BASEBLOCKEX* pexblock = recBlocks[blockidx + 1];
	if (pexblock)
		ceiling = pexblock->startpc;

	int toRemoveLast = blockidx;

	while (pexblock = recBlocks[blockidx])
	{
		u32 blockstart    = pexblock->startpc;
		u32 blockend      = pexblock->startpc + pexblock->size * 4;
		BASEBLOCK* pblock = PC_GETBLOCK(blockstart);

		if (pblock == s_pCurBlock)
		{
			if(toRemoveLast != blockidx)
				recBlocks.Remove((blockidx + 1), toRemoveLast);
			toRemoveLast = --blockidx;
			continue;
		}

		if (blockend <= addr) {
			lowerextent = std::max(lowerextent, blockend);
			break;
		}

		lowerextent = std::min(lowerextent, blockstart);
		upperextent = std::max(upperextent, blockend);
		// This might end up inside a block that doesn't contain the clearing range,
		// so set it to recompile now.  This will become JITCompile if we clear it.
		pblock->m_pFnptr = (uptr)JITCompileInBlock;

		blockidx--;
	}

	if(toRemoveLast != blockidx)
		recBlocks.Remove((blockidx + 1), toRemoveLast);

	upperextent = std::min(upperextent, ceiling);

	if (upperextent > lowerextent)
	{
		BASEBLOCK *base = (BASEBLOCK*)PC_GETBLOCK(lowerextent);
		int memsize     = upperextent - lowerextent;
		for (int i = 0; i < memsize/(int)sizeof(uptr); i++)
			base[i].m_pFnptr = (uptr)JITCompile;
	}
}

static void recReserveCache(void)
{
	if (!recMem) recMem = new RecompiledCodeReserve(_16mb);

	while (!recMem->GetPtr())
	{
		if (recMem->Reserve(GetVmMemory().MainMemory(), HostMemoryMap::EErecOffset, m_ConfiguredCacheReserve * _1mb) != NULL) break;

		// If it failed, then try again (if possible):
		if (m_ConfiguredCacheReserve < 16) break;
		m_ConfiguredCacheReserve /= 2;
	}
}

static void recReserve(void)
{
	// Hardware Requirements Check...

        /* TODO/FIXME - add SSE2 requirement check,
           and if not there, exit gracefully instead
           of doing an exception throw */

	recReserveCache();
}

static void recShutdown(void)
{
	safe_delete( recMem );
	safe_aligned_free( recRAMCopy );
	safe_aligned_free( recLutReserve_RAM );

	recBlocks.Reset();

	recRAM = recROM = recROM1 = recROM2 = NULL;

	safe_aligned_free( recConstBuf );
	safe_free( s_pInstCache );
	s_nInstCacheSize = 0;
}

static void recResetEE(void)
{
	if (eeCpuExecuting)
	{
		eeRecNeedsReset = true;
		return;
	}

	recResetRaw();
}

static fastjmp_buf m_SetJmp_StateCheck;

static void recExitExecution(void)
{
	// Without SEH we'll need to hop to a safehouse point outside the scope of recompiled
	// code.  C++ exceptions can't cross the mighty chasm in the stackframe that the recompiler
	// creates.  However, the longjump is slow so we only want to do one when absolutely
	// necessary:
	fastjmp_jmp(&m_SetJmp_StateCheck, 1 );
}

static void recCheckExecutionState(void)
{
	if( eeRecIsReset || GetCoreThread().HasPendingStateChangeRequest() )
		recExitExecution();
}

static void recExecute(void)
{
	int oldstate;

	// setjmp will save the register context and will return 0
	// A call to longjmp will restore the context (included the eip/rip)
	// but will return the longjmp 2nd parameter (here 1)
	if( !fastjmp_set(&m_SetJmp_StateCheck ) )
	{
		eeRecIsReset = false;
		eeCpuExecuting = true;

		// Important! Most of the console logging and such has cancel points in it.  This is great
		// in Windows, where SEH lets us safely kill a thread from anywhere we want.  This is bad
		// in Linux, which cannot have a C++ exception cross the recompiler.  Hence the changing
		// of the cancelstate here!

		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldstate );
		EnterRecompiledCode();

		// Generally unreachable code here ...
	}
	else
		pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, &oldstate );

	eeCpuExecuting = false;
}

////////////////////////////////////////////////////
void R5900::Dynarec::OpcodeImpl::recSYSCALL(void)
{
	recCall(R5900::Interpreter::OpcodeImpl::SYSCALL);

	xCMP(ptr32[&cpuRegs.pc], pc);
	j8Ptr[0] = JE8(0);
	xADD(ptr32[&cpuRegs.cycle], scaleblockcycles_calculation());
	// Note: technically the address is 0x8000_0180 (or 0x180)
	// (if CPU is booted)
	xJMP( (void*)DispatcherReg );
	x86SetJ8(j8Ptr[0]);
}

////////////////////////////////////////////////////
void R5900::Dynarec::OpcodeImpl::recBREAK(void)
{
	recCall(R5900::Interpreter::OpcodeImpl::BREAK);

	xCMP(ptr32[&cpuRegs.pc], pc);
	j8Ptr[0] = JE8(0);
	xADD(ptr32[&cpuRegs.cycle], scaleblockcycles_calculation());
	xJMP( (void*)DispatcherEvent );
	x86SetJ8(j8Ptr[0]);
}

void SetBranchReg( u32 reg )
{
	g_branch = 1;

	if( reg != 0xffffffff )
	{
		_allocX86reg(calleeSavedReg2d, X86TYPE_PCWRITEBACK, 0, MODE_WRITE);
		_eeMoveGPRtoR(calleeSavedReg2d, reg);

		if (EmuConfig.Gamefixes.GoemonTlbHack) {
			xMOV(ecx, calleeSavedReg2d);
			vtlb_DynV2P();
			xMOV(calleeSavedReg2d, eax);
		}

		recompileNextInstruction(1);

		if( x86regs[calleeSavedReg2d.GetId()].inuse )
		{
			xMOV(ptr[&cpuRegs.pc], calleeSavedReg2d);
			x86regs[calleeSavedReg2d.GetId()].inuse = 0;
		}
		else
		{
			xMOV(eax, ptr[&cpuRegs.pcWriteback]);
			xMOV(ptr[&cpuRegs.pc], eax);
		}
	}

	iFlushCall(FLUSH_EVERYTHING);

	iBranchTest(0xffffffff);
}

void SetBranchImm( u32 imm )
{
	g_branch = 1;

	// end the current block
	iFlushCall(FLUSH_EVERYTHING);
	xMOV(ptr32[&cpuRegs.pc], imm);
	iBranchTest(imm);
}

void SaveBranchState(void)
{
	s_savenBlockCycles = s_nBlockCycles;
	memcpy(s_saveConstRegs, g_cpuConstRegs, sizeof(g_cpuConstRegs));
	s_saveHasConstReg = g_cpuHasConstReg;
	s_saveFlushedConstReg = g_cpuFlushedConstReg;
	s_psaveInstInfo = g_pCurInstInfo;

	memcpy(s_saveXMMregs, xmmregs, sizeof(xmmregs));
}

void LoadBranchState(void)
{
	s_nBlockCycles       = s_savenBlockCycles;

	memcpy(g_cpuConstRegs, s_saveConstRegs, sizeof(g_cpuConstRegs));
	g_cpuHasConstReg     = s_saveHasConstReg;
	g_cpuFlushedConstReg = s_saveFlushedConstReg;
	g_pCurInstInfo       = s_psaveInstInfo;

	memcpy(xmmregs, s_saveXMMregs, sizeof(xmmregs));
}

void iFlushCall(int flushtype)
{
	// Free registers that are not saved across function calls (x86-32 ABI):
	_freeX86reg(eax);
	_freeX86reg(ecx);
	_freeX86reg(edx);

	if ((flushtype & FLUSH_PC) && !g_cpuFlushedPC) {
		xMOV(ptr32[&cpuRegs.pc], pc);
		g_cpuFlushedPC = true;
	}

	if ((flushtype & FLUSH_CODE) && !g_cpuFlushedCode) {
		xMOV(ptr32[&cpuRegs.code], cpuRegs.code);
		g_cpuFlushedCode = true;
	}

	if ((flushtype == FLUSH_CAUSE) && !g_maySignalException) {
		if (g_recompilingDelaySlot)
			xOR(ptr32[&cpuRegs.CP0.n.Cause], 1 << 31); // BD
		g_maySignalException = true;
	}

	if( flushtype & FLUSH_FREE_XMM )
		_freeXMMregs();
	else if( flushtype & FLUSH_FLUSH_XMM)
		_flushXMMregs();

	if( flushtype & FLUSH_CACHED_REGS )
		_flushConstRegs();
}

u32 scaleblockcycles_clear(void)
{
	u32 scaled   = scaleblockcycles_calculation();
	s8 cyclerate = g_Conf->EmuOptions.Speedhacks.EECycleRate;

	if (cyclerate > 1)
		s_nBlockCycles &= (0x1 << (cyclerate + 2)) - 1;
	else
		s_nBlockCycles &= 0x7;

	return scaled;
}

void recompileNextInstruction(int delayslot)
{
	u32 i;
	int count;
	static int *s_pCode;
	s_pCode = (int *)PSM( pc );

	// acts as a tag for delimiting recompiled instructions when viewing x86 disasm.

	cpuRegs.code = *(int *)s_pCode;

	if (!delayslot) {
		pc += 4;
		g_cpuFlushedPC = false;
		g_cpuFlushedCode = false;
	} else {
		// increment after recompiling so that pc points to the branch during recompilation
		g_recompilingDelaySlot = true;
	}

	g_pCurInstInfo++;

	for(i = 0; i < iREGCNT_XMM; ++i) {
		if( xmmregs[i].inuse ) {
			count = _recIsRegWritten(g_pCurInstInfo, (s_nEndBlock-pc)/4 + 1, xmmregs[i].type, xmmregs[i].reg);
			if( count > 0 ) xmmregs[i].counter = 1000-count;
			else xmmregs[i].counter = 0;
		}
	}

	const OPCODE& opcode = GetCurrentInstruction();

	// if this instruction is a jump or a branch, exit right away
	if( delayslot ) {
		bool check_branch_delay = false;
		switch(_Opcode_) {
			case 0:
				switch (_Funct_)
				{
					case 8: // jr
					case 9: // jalr
						check_branch_delay = true;
						break;
				}
				break;
			case 1:
				switch(_Rt_) {
					case 0: case 1: case 2: case 3: case 0x10: case 0x11: case 0x12: case 0x13:
						check_branch_delay = true;
				}
				break;

			case 2: case 3: case 4: case 5: case 6: case 7: case 0x14: case 0x15: case 0x16: case 0x17:
				check_branch_delay = true;
				break;
		}
		// Check for branch in delay slot, new code by FlatOut.
		// Gregory tested this in 2017 using the ps2autotests suite and remarked "So far we return 1 (even with this PR), and the HW 2.
		// Original PR and discussion at https://github.com/PCSX2/pcsx2/pull/1783 so we don't forget this information.
		if (check_branch_delay) {
			_clearNeededX86regs();
			_clearNeededXMMregs();
			pc += 4;
			g_cpuFlushedPC = false;
			g_cpuFlushedCode = false;
			if (g_maySignalException)
				xAND(ptr32[&cpuRegs.CP0.n.Cause], ~(1 << 31)); // BD

			g_recompilingDelaySlot = false;
			return;
		}
	}
	// Check for NOP
	if (cpuRegs.code == 0x00000000) {
		// Note: Tests on a ps2 suggested more like 5 cycles for a NOP. But there's many factors in this..
		s_nBlockCycles +=9 * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
	}
	else {
		//If the COP0 DIE bit is disabled, cycles should be doubled.
		s_nBlockCycles += opcode.cycles * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
		try {
			opcode.recompile();
		} catch (Exception::FailedToAllocateRegister&) {
			// Fall back to the interpreter
			recCall(opcode.interpret);
		}
	}

	if (!delayslot && (_getNumXMMwrite() > 2))
		_flushXMMunused();

	_clearNeededX86regs();
	_clearNeededXMMregs();

	if (delayslot) {
		pc += 4;
		g_cpuFlushedPC = false;
		g_cpuFlushedCode = false;
		if (g_maySignalException)
			xAND(ptr32[&cpuRegs.CP0.n.Cause], ~(1 << 31)); // BD
		g_recompilingDelaySlot = false;
	}

	g_maySignalException = false;

	if (!delayslot && (xGetPtr() - recPtr > 0x1000) )
		s_nEndBlock = pc;
}

static void recSetCacheReserve( uint reserveInMegs )
{
	m_ConfiguredCacheReserve = reserveInMegs;
}

static uint recGetCacheReserve(void)
{
	return m_ConfiguredCacheReserve;
}

R5900cpu recCpu =
{
	recReserve,
	recShutdown,

	recResetEE,
	recExecute,

	recCheckExecutionState,
	recClear,

	recGetCacheReserve,
	recSetCacheReserve,
};
