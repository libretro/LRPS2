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


#include "R3000A.h"
#include "Common.h"
#include "App.h" // For host irx injection hack
#include "Config.h"

#include "IopBios.h"
#include "IopHw.h"

using namespace R3000A;

// Used to flag delay slot instructions when throwig exceptions.
bool iopIsDelaySlot = false;

static bool branch2 = 0;
static u32 branchPC;

static void doBranch(s32 tar);	// forward declared prototype

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

void psxBGEZ(void)         // Branch if Rs >= 0
{
	if (_i32(_rRs_) >= 0) doBranch(_BranchTarget_);
}

void psxBGEZAL(void)   // Branch if Rs >= 0 and link
{
	_SetLink(31);
	if (_i32(_rRs_) >= 0)
	{
		doBranch(_BranchTarget_);
	}
}

void psxBGTZ(void)          // Branch if Rs >  0
{
	if (_i32(_rRs_) > 0) doBranch(_BranchTarget_);
}

void psxBLEZ(void)         // Branch if Rs <= 0
{
	if (_i32(_rRs_) <= 0) doBranch(_BranchTarget_);
}
void psxBLTZ(void)          // Branch if Rs <  0
{
	if (_i32(_rRs_) < 0) doBranch(_BranchTarget_);
}

void psxBLTZAL(void)    // Branch if Rs <  0 and link
{
	_SetLink(31);
	if (_i32(_rRs_) < 0)
		{
			doBranch(_BranchTarget_);
		}
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/

void psxBEQ(void)   // Branch if Rs == Rt
{
	if (_i32(_rRs_) == _i32(_rRt_)) doBranch(_BranchTarget_);
}

void psxBNE(void)   // Branch if Rs != Rt
{
	if (_i32(_rRs_) != _i32(_rRt_)) doBranch(_BranchTarget_);
}

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void psxJ(void)
{
	// check for iop module import table magic
	u32 delayslot = iopMemRead32(psxRegs.pc);
	if (delayslot >> 16 == 0x2400 && irxImportExec(irxImportTableAddr(psxRegs.pc), delayslot & 0xffff))
		return;

	doBranch(_JumpTarget_);
}

void psxJAL(void)
{
	_SetLink(31);
	doBranch(_JumpTarget_);
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void psxJR(void)
{
	doBranch(_u32(_rRs_));
}

void psxJALR(void)
{
	if (_Rd_)
	{
		_SetLink(_Rd_);
	}
	doBranch(_u32(_rRs_));
}

///////////////////////////////////////////
// These macros are used to assemble the reassembler functions

static __fi void R3000AexecI(void)
{
	// Inject IRX hack
	if (psxRegs.pc == 0x1630 && g_Conf->CurrentIRX.Length() > 3) {
		if (iopMemRead32(0x20018) == 0x1F) {
			// FIXME do I need to increase the module count (0x1F -> 0x20)
			iopMemWrite32(0x20094, 0xbffc0000);
		}
	}

	psxRegs.code = iopMemRead32(psxRegs.pc);
	psxRegs.pc+= 4;
	psxRegs.cycle++;
	
	/* One of the IOP to EE delta clocks to be set in PS1 mode. */
	if ((psxHu32(HW_ICFG) & (1 << 3)))
		psxRegs.iopCycleEE -= 9;
	else /* Default PS2 mode value */
		psxRegs.iopCycleEE -= 8;
	psxBSC[psxRegs.code >> 26]();
}

static void doBranch(s32 tar)
{
	branch2 = iopIsDelaySlot = true;
	branchPC = tar;
	R3000AexecI();
	iopIsDelaySlot = false;
	psxRegs.pc = branchPC;

	iopEventTest();
}

static void intReserve(void) {
}

static void intAlloc(void) {
}

static void intReset(void) {
	intAlloc();
}

static void intExecute(void)
{
	for (;;)
		R3000AexecI();
}

static s32 intExecuteBlock( s32 eeCycles )
{
	psxRegs.iopBreak   = 0;
	psxRegs.iopCycleEE = eeCycles;

	while (psxRegs.iopCycleEE > 0)
	{
		if ((psxHu32(HW_ICFG) & 8) && ((psxRegs.pc & 0x1fffffffU) == 0xa0 || (psxRegs.pc & 0x1fffffffU) == 0xb0 || (psxRegs.pc & 0x1fffffffU) == 0xc0))
			psxBiosCall();

		branch2 = 0;
		while (!branch2)
			R3000AexecI();
	}
	return psxRegs.iopBreak + psxRegs.iopCycleEE;
}

static void intClear(u32 Addr, u32 Size) { }

static void intShutdown(void) { }

static void intSetCacheReserve( uint reserveInMegs ) { }

static uint intGetCacheReserve(void) { return 0; }

R3000Acpu psxInt = {
	intReserve,
	intReset,
	intExecute,
	intExecuteBlock,
	intClear,
	intShutdown,

	intGetCacheReserve,
	intSetCacheReserve
};
