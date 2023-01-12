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


// recompiler reworked to add dynamic linking zerofrog(@gmail.com) Jan06

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

using namespace x86Emitter;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl
{

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#ifndef BRANCH_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_SYS(BEQ);
REC_SYS(BEQL);
REC_SYS(BNE);
REC_SYS(BNEL);
REC_SYS(BLTZ);
REC_SYS(BGTZ);
REC_SYS(BLEZ);
REC_SYS(BGEZ);
REC_SYS(BGTZL);
REC_SYS(BLTZL);
REC_SYS_DEL(BLTZAL, 31);
REC_SYS_DEL(BLTZALL, 31);
REC_SYS(BLEZL);
REC_SYS(BGEZL);
REC_SYS_DEL(BGEZAL, 31);
REC_SYS_DEL(BGEZALL, 31);

#else

void recSetBranchEQ(int info, int bne, int process)
{
	if( info & PROCESS_EE_XMM ) {
		int t0reg;

		if( process & PROCESS_CONSTS ) {
			if( (g_pCurInstInfo->regs[_Rt_] & EEINST_LASTUSE) || !EEINST_ISLIVEXMM(_Rt_) ) {
				_deleteGPRtoXMMreg(_Rt_, 1);
				xmmregs[EEREC_T].inuse = 0;
				t0reg = EEREC_T;
			}
			else {
				t0reg = _allocTempXMMreg(XMMT_INT, -1);
				xMOVQZX(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
			}

			_flushConstReg(_Rs_);
			xPCMP.EQD(xRegisterSSE(t0reg), ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);


			if( t0reg != EEREC_T ) _freeXMMreg(t0reg);
		}
		else if( process & PROCESS_CONSTT ) {
			if( (g_pCurInstInfo->regs[_Rs_] & EEINST_LASTUSE) || !EEINST_ISLIVEXMM(_Rs_) ) {
				_deleteGPRtoXMMreg(_Rs_, 1);
				xmmregs[EEREC_S].inuse = 0;
				t0reg = EEREC_S;
			}
			else {
				t0reg = _allocTempXMMreg(XMMT_INT, -1);
				xMOVQZX(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			}

			_flushConstReg(_Rt_);
			xPCMP.EQD(xRegisterSSE(t0reg), ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);

			if( t0reg != EEREC_S ) _freeXMMreg(t0reg);
		}
		else {

			if( (g_pCurInstInfo->regs[_Rs_] & EEINST_LASTUSE) || !EEINST_ISLIVEXMM(_Rs_) ) {
				_deleteGPRtoXMMreg(_Rs_, 1);
				xmmregs[EEREC_S].inuse = 0;
				t0reg = EEREC_S;
				xPCMP.EQD(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
			}
			else if( (g_pCurInstInfo->regs[_Rt_] & EEINST_LASTUSE) || !EEINST_ISLIVEXMM(_Rt_) ) {
				_deleteGPRtoXMMreg(_Rt_, 1);
				xmmregs[EEREC_T].inuse = 0;
				t0reg = EEREC_T;
				xPCMP.EQD(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			}
			else {
				t0reg = _allocTempXMMreg(XMMT_INT, -1);
				xMOVQZX(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
				xPCMP.EQD(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
			}

			if( t0reg != EEREC_S && t0reg != EEREC_T ) _freeXMMreg(t0reg);
		}

		xMOVMSKPS(eax, xRegisterSSE(t0reg));

		_eeFlushAllUnused();

		xAND(al, 3);
		xCMP(al, 0x3 );

		if( bne ) j32Ptr[ 1 ] = JE32( 0 );
		else j32Ptr[ 0 ] = j32Ptr[ 1 ] = JNE32( 0 );
	}
	else {

		_eeFlushAllUnused();

		if( bne ) {
			if( process & PROCESS_CONSTS ) {
				xCMP(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ]], g_cpuConstRegs[_Rs_].UL[0] );
				j8Ptr[ 0 ] = JNE8( 0 );

				xCMP(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ]], g_cpuConstRegs[_Rs_].UL[1] );
				j32Ptr[ 1 ] = JE32( 0 );
			}
			else if( process & PROCESS_CONSTT ) {
				xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]], g_cpuConstRegs[_Rt_].UL[0] );
				j8Ptr[ 0 ] = JNE8( 0 );

				xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ]], g_cpuConstRegs[_Rt_].UL[1] );
				j32Ptr[ 1 ] = JE32( 0 );
			}
			else {
				xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
				xCMP(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
				j8Ptr[ 0 ] = JNE8( 0 );

				xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ] ]);
				xCMP(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ] ]);
				j32Ptr[ 1 ] = JE32( 0 );
			}

			x86SetJ8( j8Ptr[0] );
		}
		else {
			// beq
			if( process & PROCESS_CONSTS ) {
				xCMP(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ]], g_cpuConstRegs[_Rs_].UL[0] );
				j32Ptr[ 0 ] = JNE32( 0 );

				xCMP(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ]], g_cpuConstRegs[_Rs_].UL[1] );
				j32Ptr[ 1 ] = JNE32( 0 );
			}
			else if( process & PROCESS_CONSTT ) {
				xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]], g_cpuConstRegs[_Rt_].UL[0] );
				j32Ptr[ 0 ] = JNE32( 0 );

				xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ]], g_cpuConstRegs[_Rt_].UL[1] );
				j32Ptr[ 1 ] = JNE32( 0 );
			}
			else {
				xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
				xCMP(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
				j32Ptr[ 0 ] = JNE32( 0 );

				xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ] ]);
				xCMP(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ] ]);
				j32Ptr[ 1 ] = JNE32( 0 );
			}
		}
	}

	_clearNeededXMMregs();
}

void recSetBranchL(int ltz)
{
	int regs = _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ);

	if( regs >= 0 ) {
		xMOVMSKPS(eax, xRegisterSSE(regs));

		_eeFlushAllUnused();

		xTEST(al, 2 );

		if( ltz ) j32Ptr[ 0 ] = JZ32( 0 );
		else j32Ptr[ 0 ] = JNZ32( 0 );

		return;
	}

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ]], 0 );
	if( ltz ) j32Ptr[ 0 ] = JGE32( 0 );
	else j32Ptr[ 0 ] = JL32( 0 );

	_clearNeededXMMregs();
}

//// BEQ
void recBEQ_const()
{
	u32 branchTo;

	if( g_cpuConstRegs[_Rs_].SD[0] == g_cpuConstRegs[_Rt_].SD[0] )
		branchTo = ((s32)_Imm_ * 4) + pc;
	else
		branchTo = pc+4;

	recompileNextInstruction(1);
	SetBranchImm( branchTo );
}

void recBEQ_process(int info, int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if ( _Rs_ == _Rt_ )
	{
		recompileNextInstruction(1);
		SetBranchImm( branchTo );
	}
	else
	{
		recSetBranchEQ(info, 0, process);

		SaveBranchState();
		recompileNextInstruction(1);

		SetBranchImm(branchTo);

		x86SetJ32( j32Ptr[ 0 ] );
		x86SetJ32( j32Ptr[ 1 ] );

		// recopy the next inst
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(1);

		SetBranchImm(pc);
	}
}

void recBEQ_(int info) { recBEQ_process(info, 0); }
void recBEQ_consts(int info) { recBEQ_process(info, PROCESS_CONSTS); }
void recBEQ_constt(int info) { recBEQ_process(info, PROCESS_CONSTT); }

EERECOMPILE_CODE0(BEQ, XMMINFO_READS|XMMINFO_READT);

//// BNE
void recBNE_const()
{
	u32 branchTo;

	if( g_cpuConstRegs[_Rs_].SD[0] != g_cpuConstRegs[_Rt_].SD[0] )
		branchTo = ((s32)_Imm_ * 4) + pc;
	else
		branchTo = pc+4;

	recompileNextInstruction(1);
	SetBranchImm( branchTo );
}

void recBNE_process(int info, int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if ( _Rs_ == _Rt_ )
	{
		recompileNextInstruction(1);
		SetBranchImm(pc);
		return;
	}

	recSetBranchEQ(info, 1, process);

	SaveBranchState();
	recompileNextInstruction(1);

	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 1 ] );

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
}

void recBNE_(int info) { recBNE_process(info, 0); }
void recBNE_consts(int info) { recBNE_process(info, PROCESS_CONSTS); }
void recBNE_constt(int info) { recBNE_process(info, PROCESS_CONSTT); }

EERECOMPILE_CODE0(BNE, XMMINFO_READS|XMMINFO_READT);

//// BEQL
void recBEQL_const()
{
	if( g_cpuConstRegs[_Rs_].SD[0] == g_cpuConstRegs[_Rt_].SD[0] ) {
		u32 branchTo = ((s32)_Imm_ * 4) + pc;
		recompileNextInstruction(1);
		SetBranchImm( branchTo );
	}
	else {
		SetBranchImm( pc+4 );
	}
}

void recBEQL_process(int info, int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;
	recSetBranchEQ(info, 0, process);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );
	x86SetJ32( j32Ptr[ 1 ] );

	LoadBranchState();
	SetBranchImm(pc);
}

void recBEQL_(int info) { recBEQL_process(info, 0); }
void recBEQL_consts(int info) { recBEQL_process(info, PROCESS_CONSTS); }
void recBEQL_constt(int info) { recBEQL_process(info, PROCESS_CONSTT); }

EERECOMPILE_CODE0(BEQL, XMMINFO_READS|XMMINFO_READT);

//// BNEL
void recBNEL_const()
{
	if( g_cpuConstRegs[_Rs_].SD[0] != g_cpuConstRegs[_Rt_].SD[0] ) {
		u32 branchTo = ((s32)_Imm_ * 4) + pc;
		recompileNextInstruction(1);
		SetBranchImm(branchTo);
	}
	else {
		SetBranchImm( pc+4 );
	}
}

void recBNEL_process(int info, int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	recSetBranchEQ(info, 0, process);

	SaveBranchState();
	SetBranchImm(pc+4);

	x86SetJ32( j32Ptr[ 0 ] );
	x86SetJ32( j32Ptr[ 1 ] );

	// recopy the next inst
	LoadBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);
}

void recBNEL_(int info) { recBNEL_process(info, 0); }
void recBNEL_consts(int info) { recBNEL_process(info, PROCESS_CONSTS); }
void recBNEL_constt(int info) { recBNEL_process(info, PROCESS_CONSTT); }

EERECOMPILE_CODE0(BNEL, XMMINFO_READS|XMMINFO_READT);

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

////////////////////////////////////////////////////
void recBLTZAL(void)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllUnused();

	_deleteEEreg(31, 0);
	xMOV(ptr32[&cpuRegs.GPR.r[31].UL[0]], pc+4);
	xMOV(ptr32[&cpuRegs.GPR.r[31].UL[1]], 0);

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] < 0) )
			branchTo = pc+4;

		recompileNextInstruction(1);
		SetBranchImm( branchTo );
		return;
	}

	recSetBranchL(1);

	SaveBranchState();

	recompileNextInstruction(1);

	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBGEZAL()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllUnused();

	_deleteEEreg(31, 0);
	xMOV(ptr32[&cpuRegs.GPR.r[31].UL[0]], pc+4);
	xMOV(ptr32[&cpuRegs.GPR.r[31].UL[1]], 0);

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] >= 0) )
			branchTo = pc+4;

		recompileNextInstruction(1);
		SetBranchImm( branchTo );
		return;
	}

	recSetBranchL(0);

	SaveBranchState();

	recompileNextInstruction(1);

	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBLTZALL()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllUnused();

	_deleteEEreg(31, 0);
	xMOV(ptr32[&cpuRegs.GPR.r[31].UL[0]], pc+4);
	xMOV(ptr32[&cpuRegs.GPR.r[31].UL[1]], 0);

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] < 0) )
			SetBranchImm( pc + 4);
		else {
			recompileNextInstruction(1);
			SetBranchImm( branchTo );
		}
		return;
	}

	recSetBranchL(1);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );

	LoadBranchState();
	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBGEZALL()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllUnused();

	_deleteEEreg(31, 0);
	xMOV(ptr32[&cpuRegs.GPR.r[31].UL[0]], pc+4);
	xMOV(ptr32[&cpuRegs.GPR.r[31].UL[1]], 0);

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] >= 0) )
			SetBranchImm( pc + 4);
		else {
			recompileNextInstruction(1);
			SetBranchImm( branchTo );
		}
		return;
	}

	recSetBranchL(0);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );

	LoadBranchState();
	SetBranchImm(pc);
}


//// BLEZ
void recBLEZ()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeFlushAllUnused();

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] <= 0) )
			branchTo = pc+4;

		recompileNextInstruction(1);
		SetBranchImm( branchTo );
		return;
	}

	_flushEEreg(_Rs_);

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ]], 0 );
	j8Ptr[ 0 ] = JL8( 0 );
	j32Ptr[ 1 ] = JG32( 0 );

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]], 0 );
	j32Ptr[ 2 ] = JNZ32( 0 );

	x86SetJ8( j8Ptr[ 0 ] );

	_clearNeededXMMregs();

	SaveBranchState();
	recompileNextInstruction(1);

	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 1 ] );
	x86SetJ32( j32Ptr[ 2 ] );

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
}

//// BGTZ
void recBGTZ()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeFlushAllUnused();

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] > 0) )
			branchTo = pc+4;

		recompileNextInstruction(1);
		SetBranchImm( branchTo );
		return;
	}

	_flushEEreg(_Rs_);

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ]], 0 );
	j8Ptr[ 0 ] = JG8( 0 );
	j32Ptr[ 1 ] = JL32( 0 );

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]], 0 );
	j32Ptr[ 2 ] = JZ32( 0 );

	x86SetJ8( j8Ptr[ 0 ] );

	_clearNeededXMMregs();

	SaveBranchState();
	recompileNextInstruction(1);

	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 1 ] );
	x86SetJ32( j32Ptr[ 2 ] );

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBLTZ()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeFlushAllUnused();

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] < 0) )
			branchTo = pc+4;

		recompileNextInstruction(1);
		SetBranchImm( branchTo );
		return;
	}

	recSetBranchL(1);

	SaveBranchState();
	recompileNextInstruction(1);

	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBGEZ()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeFlushAllUnused();

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] >= 0) )
			branchTo = pc+4;

		recompileNextInstruction(1);
		SetBranchImm( branchTo );
		return;
	}

	recSetBranchL(0);

	SaveBranchState();
	recompileNextInstruction(1);

	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBLTZL()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeFlushAllUnused();

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] < 0) )
			SetBranchImm( pc + 4);
		else {
			recompileNextInstruction(1);
			SetBranchImm( branchTo );
		}
		return;
	}

	recSetBranchL(1);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );

	LoadBranchState();
	SetBranchImm(pc);
}


////////////////////////////////////////////////////
void recBGEZL()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeFlushAllUnused();

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] >= 0) )
			SetBranchImm( pc + 4);
		else {
			recompileNextInstruction(1);
			SetBranchImm( branchTo );
		}
		return;
	}

	recSetBranchL(0);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 0 ] );

	LoadBranchState();
	SetBranchImm(pc);
}




/*********************************************************
* Register branch logic  Likely                          *
* Format:  OP rs, offset                                 *
*********************************************************/

////////////////////////////////////////////////////
void recBLEZL()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeFlushAllUnused();

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] <= 0) )
			SetBranchImm( pc + 4);
		else {
			_clearNeededXMMregs();
			recompileNextInstruction(1);
			SetBranchImm( branchTo );
		}
		return;
	}

	_flushEEreg(_Rs_);

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ]], 0 );
	j32Ptr[ 0 ] = JL32( 0 );
	j32Ptr[ 1 ] = JG32( 0 );

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]], 0 );
	j32Ptr[ 2 ] = JNZ32( 0 );

	x86SetJ32( j32Ptr[ 0 ] );

	_clearNeededXMMregs();

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 1 ] );
	x86SetJ32( j32Ptr[ 2 ] );

	LoadBranchState();
	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBGTZL()
{

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeFlushAllUnused();

	if( GPR_IS_CONST1(_Rs_) ) {
		if( !(g_cpuConstRegs[_Rs_].SD[0] > 0) )
			SetBranchImm( pc + 4);
		else {
			_clearNeededXMMregs();
			recompileNextInstruction(1);
			SetBranchImm( branchTo );
		}
		return;
	}

	_flushEEreg(_Rs_);

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ]], 0 );
	j32Ptr[ 0 ] = JG32( 0 );
	j32Ptr[ 1 ] = JL32( 0 );

	xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]], 0 );
	j32Ptr[ 2 ] = JZ32( 0 );

	x86SetJ32( j32Ptr[ 0 ] );

	_clearNeededXMMregs();

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32( j32Ptr[ 1 ] );
	x86SetJ32( j32Ptr[ 2 ] );

	LoadBranchState();
	SetBranchImm(pc);
}

#endif

} } }
