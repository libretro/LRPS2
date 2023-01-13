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
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
#ifndef JUMP_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_SYS(J);
REC_SYS_DEL(JAL, 31);
REC_SYS(JR);
REC_SYS_DEL(JALR, _Rd_);

#else

////////////////////////////////////////////////////
void recJ()
{

	// SET_FPUSTATE;
	u32 newpc = (_InstrucTarget_ << 2) + ( pc & 0xf0000000 );
	recompileNextInstruction(1);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchImm(vtlb_V2P(newpc));
	else
		SetBranchImm(newpc);
}

////////////////////////////////////////////////////
void recJAL()
{

	u32 newpc = (_InstrucTarget_ << 2) + ( pc & 0xf0000000 );
	_deleteEEreg(31, 0);
	GPR_SET_CONST(31);
	g_cpuConstRegs[31].UL[0] = pc + 4;
	g_cpuConstRegs[31].UL[1] = 0;

	recompileNextInstruction(1);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchImm(vtlb_V2P(newpc));
	else
		SetBranchImm(newpc);
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/

////////////////////////////////////////////////////
void recJR()
{

	SetBranchReg( _Rs_);
}

////////////////////////////////////////////////////
void recJALR()
{
	int newpc = pc + 4;
	_allocX86reg(calleeSavedReg2d, X86TYPE_PCWRITEBACK, 0, MODE_WRITE);
	_eeMoveGPRtoR(calleeSavedReg2d, _Rs_);

	if (EmuConfig.Gamefixes.GoemonTlbHack) {
		xMOV(ecx, calleeSavedReg2d);
		vtlb_DynV2P();
		xMOV(calleeSavedReg2d, eax);
	}
	
	if ( _Rd_ )
	{
		_deleteEEreg(_Rd_, 0);
		GPR_SET_CONST(_Rd_);
		g_cpuConstRegs[_Rd_].UL[0] = newpc;
		g_cpuConstRegs[_Rd_].UL[1] = 0;
	}

	_clearNeededXMMregs();
	recompileNextInstruction(1);

	if( x86regs[calleeSavedReg2d.GetId()].inuse ) {
		xMOV(ptr[&cpuRegs.pc], calleeSavedReg2d);
		x86regs[calleeSavedReg2d.GetId()].inuse = 0;
	}
	else {
		xMOV(eax, ptr[&cpuRegs.pcWriteback]);
		xMOV(ptr[&cpuRegs.pc], eax);
	}

	SetBranchReg(0xffffffff);
}

#endif

} } }
