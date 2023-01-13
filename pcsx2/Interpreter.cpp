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

#include "R5900OpcodeTables.h"
#include "R5900Exceptions.h"
#include "gui/SysThreads.h"

#include "Elfheader.h"

#include <float.h>

static u32 cpuBlockCycles = 0;	// 3 bit fixed point version of cycle count

/* Perform counters, ints, and IOP updates */
#define intEventTest() _cpuEventTest_Shared()

static void R5900execI(void)
{
	u32 pc = cpuRegs.pc;
	// We need to increase the pc before executing the memRead32. An exception could appears
	// and it expects the PC counter to be pre-incremented
	cpuRegs.pc += 4;

	// interprete instruction
	cpuRegs.code = memRead32( pc );

	const R5900::OPCODE& opcode = R5900::GetCurrentInstruction();

	cpuBlockCycles += opcode.cycles;

	opcode.interpret();
}

static __fi void _doBranch_shared(u32 tar)
{
	cpuRegs.branch = 1;
	R5900execI();

	// branch being 0 means an exception was thrown, since only the exception
	// handler should ever clear it.

	if( cpuRegs.branch != 0 )
	{
		cpuRegs.pc = tar;
		cpuRegs.branch = 0;
	}
}

static void doBranch( u32 target )
{
	_doBranch_shared( target );
	cpuRegs.cycle  += cpuBlockCycles >> 3;
	cpuBlockCycles &= (1<<3)-1;
	intEventTest();
}

void intDoBranch(u32 target)
{
	_doBranch_shared( target );

	if( Cpu == &intCpu )
	{
		cpuRegs.cycle += cpuBlockCycles >> 3;
		cpuBlockCycles &= (1<<3)-1;
		intEventTest();
	}
}

////////////////////////////////////////////////////////////////////
// R5900 Branching Instructions!
// These are the interpreter versions of the branch instructions.  Unlike other
// types of interpreter instructions which can be called safely from the recompilers,
// these instructions are not "recSafe" because they may not invoke the
// necessary branch test logic that the recs need to maintain sync with the
// cpuRegs.pc and delaySlot instruction and such.

namespace R5900 {
namespace Interpreter {
namespace OpcodeImpl {

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
// fixme: looking at the other branching code, shouldn't those _SetLinks in BGEZAL and such only be set
// if the condition is true? --arcum42

void J(void)
{
	doBranch(_JumpTarget_);
}

void JAL(void)
{
	// 0x3563b8 is the start address of the function that invalidate entry in TLB cache
	if (EmuConfig.Gamefixes.GoemonTlbHack) {
		if (_JumpTarget_ == 0x3563b8)
			GoemonUnloadTlb(cpuRegs.GPR.n.a0.UL[0]);
	}
	_SetLink(31);
	doBranch(_JumpTarget_);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/

void BEQ(void)  // Branch if Rs == Rt
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] == cpuRegs.GPR.r[_Rt_].SD[0])
		doBranch(_BranchTarget_);
	else
		intEventTest();
}

void BNE(void)  // Branch if Rs != Rt
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] != cpuRegs.GPR.r[_Rt_].SD[0])
		doBranch(_BranchTarget_);
	else
		intEventTest();
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

void BGEZ(void)    // Branch if Rs >= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
		doBranch(_BranchTarget_);
}

void BGEZAL(void) // Branch if Rs >= 0 and link
{
	_SetLink(31);
	if (cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
		doBranch(_BranchTarget_);
}

void BGTZ(void)    // Branch if Rs >  0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] > 0)
		doBranch(_BranchTarget_);
}

void BLEZ(void)   // Branch if Rs <= 0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] <= 0)
		doBranch(_BranchTarget_);
}

void BLTZ(void)    // Branch if Rs <  0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] < 0)
		doBranch(_BranchTarget_);
}

void BLTZAL(void)  // Branch if Rs <  0 and link
{
	_SetLink(31);
	if (cpuRegs.GPR.r[_Rs_].SD[0] < 0)
		doBranch(_BranchTarget_);
}

/*********************************************************
* Register branch logic  Likely                          *
* Format:  OP rs, offset                                 *
*********************************************************/


void BEQL(void)    // Branch if Rs == Rt
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] == cpuRegs.GPR.r[_Rt_].SD[0])
		doBranch(_BranchTarget_);
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BNEL(void)     // Branch if Rs != Rt
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] != cpuRegs.GPR.r[_Rt_].SD[0])
		doBranch(_BranchTarget_);
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLEZL(void)    // Branch if Rs <= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] <= 0)
		doBranch(_BranchTarget_);
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGTZL(void)     // Branch if Rs >  0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] > 0)
		doBranch(_BranchTarget_);
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLTZL(void)     // Branch if Rs <  0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] < 0)
		doBranch(_BranchTarget_);
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGEZL(void)     // Branch if Rs >= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLTZALL(void)   // Branch if Rs <  0 and link
{
	_SetLink(31);
	if(cpuRegs.GPR.r[_Rs_].SD[0] < 0)
		doBranch(_BranchTarget_);
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGEZALL(void)   // Branch if Rs >= 0 and link
{
	_SetLink(31);
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
		doBranch(_BranchTarget_);
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void JR(void)
{
	// 0x33ad48 and 0x35060c are the return address of the function (0x356250) that populate the TLB cache
	if (EmuConfig.Gamefixes.GoemonTlbHack) {
		u32 add = cpuRegs.GPR.r[_Rs_].UL[0];
		if (add == 0x33ad48 || add == 0x35060c)
			GoemonPreloadTlb();
	}
	doBranch(cpuRegs.GPR.r[_Rs_].UL[0]);
}

void JALR(void)
{
	u32 temp = cpuRegs.GPR.r[_Rs_].UL[0];

	if (_Rd_)  _SetLink(_Rd_);

	doBranch(temp);
}

} } }		// end namespace R5900::Interpreter::OpcodeImpl


// --------------------------------------------------------------------------------------
//  R5900cpu/intCpu interface (implementations)
// --------------------------------------------------------------------------------------
static void intDummyFunc(void) { }
static void intReset(void) { cpuRegs.branch = 0; }

static void intExecute(void)
{
	bool instruction_was_cancelled;
	enum ExecuteState {
		RESET,
		GAME_LOADING,
		GAME_RUNNING
	};
	ExecuteState state = RESET;
	do {
		instruction_was_cancelled = false;
		try {
			// The execution was splited in three parts so it is easier to
			// resume it after a cancelled instruction.
			switch (state) {
				case RESET:
					do
						R5900execI();
					while (cpuRegs.pc != (g_eeloadMain ? g_eeloadMain : EELOAD_START));
					if (cpuRegs.pc == EELOAD_START)
					{
						// The EELOAD _start function is the same across all BIOS versions afaik
						u32 mainjump = memRead32(EELOAD_START + 0x9c);
						if (mainjump >> 26 == 3) // JAL
							g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | (mainjump << 2 & 0x0fffffffU);
					}
					else if (cpuRegs.pc == g_eeloadMain)
					{
						eeloadHook();
						if (g_SkipBiosHack)
						{
							// See comments on this code in iR5900-32.cpp's recRecompile()
							u32 typeAexecjump = memRead32(EELOAD_START + 0x470);
							u32 typeBexecjump = memRead32(EELOAD_START + 0x5B0);
							u32 typeCexecjump = memRead32(EELOAD_START + 0x618);
							u32 typeDexecjump = memRead32(EELOAD_START + 0x600);
							if ((typeBexecjump >> 26 == 3) || (typeCexecjump >> 26 == 3) || (typeDexecjump >> 26 == 3)) // JAL to 0x822B8
								g_eeloadExec = EELOAD_START + 0x2B8;
							else if (typeAexecjump >> 26 == 3) // JAL to 0x82170
								g_eeloadExec = EELOAD_START + 0x170;
						}
					}
					else if (cpuRegs.pc == g_eeloadExec)
						eeloadHook2();

					if (g_GameLoading)
						state = GAME_LOADING;
					else
						break;

				case GAME_LOADING:
					if (ElfEntry != 0xFFFFFFFF) {
						do
							R5900execI();
						while (cpuRegs.pc != ElfEntry);
						eeGameStarting();
					}
					state = GAME_RUNNING;

				case GAME_RUNNING:
					for (;;)
						R5900execI();
			}
		}
		catch( Exception::ExitCpuExecute& ) { }
		catch( Exception::CancelInstruction& ) { instruction_was_cancelled = true; }

		// For example a tlb miss will throw an exception. Cpu must be resumed
		// to execute the handler
	} while (instruction_was_cancelled);
}

static void intCheckExecutionState(void)
{
	if( GetCoreThread().HasPendingStateChangeRequest() )
		throw Exception::ExitCpuExecute();
}

static void intClear(u32 Addr, u32 Size) { }
static void intSetCacheReserve( uint reserveInMegs ) { }
static uint intGetCacheReserve(void) { return 0; }

R5900cpu intCpu =
{
	intDummyFunc,
	intDummyFunc,

	intReset,
	intExecute,

	intCheckExecutionState,
	intClear,

	intGetCacheReserve,
	intSetCacheReserve,
};
