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

#include "Sio.h"
#include "Sif.h"
#include "IopSio2.h"
#include "IopCounters.h"
#include "IopBios.h"
#include "IopHw.h"
#include "IopDma.h"
#include "CDVD/CdRom.h"
#include "CDVD/CDVD.h"

using namespace R3000A;

R3000Acpu *psxCpu;

// used for constant propagation
u32 g_psxConstRegs[32];
u32 g_psxHasConstReg, g_psxFlushedConstReg;

// Used to signal to the EE when important actions that need IOP-attention have
// happened (hsyncs, vsyncs, IOP exceptions, etc).  IOP runs code whenever this
// is true, even if it's already running ahead a bit.
bool iopEventAction = false;

static bool iopEventTestIsActive = false;

__aligned16 psxRegisters psxRegs;

void psxReset(void)
{
	memzero(psxRegs);

	psxRegs.pc                 = 0xbfc00000; // Start in bootstrap
	psxRegs.CP0.n.Status       = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	psxRegs.CP0.n.PRid         = 0x0000001f; // PRevID = Revision ID, same as the IOP R3000A

	psxRegs.iopBreak           = 0;
	psxRegs.iopCycleEE         = -1;
	psxRegs.iopNextEventCycle  = psxRegs.cycle + 4;

	psxHwReset();
	PSXCLK = 36864000;
	ioman::reset();
	psxBiosReset();
}

void psxShutdown(void) { }

void psxException(u32 code, u32 bd)
{
	// Set the Cause
	psxRegs.CP0.n.Cause &= ~0x7f;
	psxRegs.CP0.n.Cause |= code;

	// Set the EPC & PC
	if (bd)
	{
		psxRegs.CP0.n.Cause|= 0x80000000;
		psxRegs.CP0.n.EPC = (psxRegs.pc - 4);
	}
	else
		psxRegs.CP0.n.EPC = (psxRegs.pc);

	if (psxRegs.CP0.n.Status & 0x400000)
		psxRegs.pc = 0xbfc00180;
	else
		psxRegs.pc = 0x80000080;

	// Set the Status
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status &~0x3f) |
		((psxRegs.CP0.n.Status & 0xf) << 2);
}

// typecast the conditional to signed so that things don't blow up
// if startCycle is greater than our next branch cycle.
__fi void psxSetNextBranch( u32 startCycle, s32 delta )
{
	if( (int)(psxRegs.iopNextEventCycle - startCycle) > delta )
		psxRegs.iopNextEventCycle = startCycle + delta;
}

__fi void psxSetNextBranchDelta( s32 delta )
{
	psxSetNextBranch( psxRegs.cycle, delta );
}

__fi int psxTestCycle( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't explode
	// if the startCycle is ahead of our current cpu cycle.

	return (int)(psxRegs.cycle - startCycle) >= delta;
}

__fi void PSX_INT( IopEventId n, s32 ecycle )
{
	psxRegs.interrupt |= 1 << n;

	psxRegs.sCycle[n] = psxRegs.cycle;
	psxRegs.eCycle[n] = ecycle;

	psxSetNextBranchDelta( ecycle );

	if (psxRegs.iopCycleEE < 0)
	{
		// The EE called this int, so inform it to branch as needed:
		// fixme - this doesn't take into account EE/IOP sync (the IOP may be running
		// ahead or behind the EE as per the EEsCycles value)
		s32 iopDelta = (psxRegs.iopNextEventCycle-psxRegs.cycle)*8;
		cpuSetNextEventDelta( iopDelta );
	}
}

static __fi void IopTestEvent( IopEventId n, void (*callback)() )
{
	if( !(psxRegs.interrupt & (1 << n)) ) return;

	if( psxTestCycle( psxRegs.sCycle[n], psxRegs.eCycle[n] ) )
	{
		psxRegs.interrupt &= ~(1 << n);
		callback();
	}
	else
		psxSetNextBranch( psxRegs.sCycle[n], psxRegs.eCycle[n] );
}

static __fi void _psxTestInterrupts(void)
{
	IopTestEvent(IopEvt_SIF0,		sif0Interrupt);	// SIF0
	IopTestEvent(IopEvt_SIF1,		sif1Interrupt);	// SIF1
	IopTestEvent(IopEvt_SIF2,		sif2Interrupt);	// SIF2
	// Originally controlled by a preprocessor define, now PSX dependent.
	if (psxHu32(HW_ICFG) & (1 << 3)) IopTestEvent(IopEvt_SIO, sioInterruptR);
	IopTestEvent(IopEvt_CdvdRead,	cdvdReadInterrupt);

	// Profile-guided Optimization (sorta)
	// The following ints are rarely called.  Encasing them in a conditional
	// as follows helps speed up most games.

	if( psxRegs.interrupt & ((1 << IopEvt_Cdvd) | (1 << IopEvt_Dma11) | (1 << IopEvt_Dma12)
		| (1 << IopEvt_Cdrom) | (1 << IopEvt_CdromRead) | (1 << IopEvt_DEV9) | (1 << IopEvt_USB)))
	{
		IopTestEvent(IopEvt_Cdvd,		cdvdActionInterrupt);
		IopTestEvent(IopEvt_Dma11,		psxDMA11Interrupt);	// SIO2
		IopTestEvent(IopEvt_Dma12,		psxDMA12Interrupt);	// SIO2
		IopTestEvent(IopEvt_Cdrom,		cdrInterrupt);
		IopTestEvent(IopEvt_CdromRead,	cdrReadInterrupt);
		IopTestEvent(IopEvt_DEV9,		dev9Interrupt);
		IopTestEvent(IopEvt_USB,		usbInterrupt);
	}
}

__ri void iopEventTest(void)
{
	if (psxTestCycle(psxNextsCounter, psxNextCounter))
	{
		psxRcntUpdate();
		iopEventAction = true;
	}
	else
		// start the next branch at the next counter event by default
		// the interrupt code below will assign nearer branches if needed.
		psxRegs.iopNextEventCycle = psxNextsCounter+psxNextCounter;


	if (psxRegs.interrupt)
	{
		iopEventTestIsActive = true;
		_psxTestInterrupts();
		iopEventTestIsActive = false;
	}

	if( (psxHu32(0x1078) != 0) && ((psxHu32(0x1070) & psxHu32(0x1074)) != 0) )
	{
		if( (psxRegs.CP0.n.Status & 0xFE01) >= 0x401 )
		{
			psxException(0, 0);
			iopEventAction = true;
		}
	}
}

void iopTestIntc(void)
{
	if( psxHu32(0x1078) == 0 ) return;
	if( (psxHu32(0x1070) & psxHu32(0x1074)) == 0 ) return;

	if( !eeEventTestIsActive )
	{
		// An iop exception has occurred while the EE is running code.
		// Inform the EE to branch so the IOP can handle it promptly:

		cpuSetNextEventDelta( 16 );
		iopEventAction = true;

		// Note: No need to set the iop's branch delta here, since the EE
		// will run an IOP branch test regardless.
	}
	else if( !iopEventTestIsActive )
		psxSetNextBranchDelta( 2 );
}
