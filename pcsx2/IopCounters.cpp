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


// Note on INTC usage: All counters code is always called from inside the context of an
// event test, so instead of using the iopTestIntc we just set the 0x1070 flags directly.
// The EventText function will pick it up.

#include "Utilities/MemcpyFast.h"
#include "R3000A.h"
#include "Common.h"
#include "SPU2/spu2.h"
#include "DEV9/DEV9.h"
#include "CDVD/CDVD.h"
#include "IopHw.h"
#include "IopDma.h"
#include "IopCounters.h"

#include <math.h>

/* Config.PsxType == 1: PAL:
	 VBlank interlaced		50.00 Hz
	 VBlank non-interlaced	49.76 Hz
	 HBlank					15.625 KHz
   Config.PsxType == 0: NSTC
	 VBlank interlaced		59.94 Hz
	 VBlank non-interlaced	59.82 Hz
	 HBlank					15.73426573 KHz */

// Misc IOP Clocks
#define PSXPIXEL ((int)(PSXCLK / 13500000))
#define PSXSOUNDCLK ((int)(48000))

psxCounter psxCounters[NUM_COUNTERS];
s32 psxNextCounter;
u32 psxNextsCounter;
u8 psxhblankgate = 0;
u8 psxvblankgate = 0;

// flags when the gate is off or counter disabled. (do not count)
#define IOPCNT_STOPPED (0x10000000ul)

// used to disable targets until after an overflow
#define IOPCNT_FUTURE_TARGET (0x1000000000ULL)

#define IOPCNT_ENABLE_GATE (1 << 0)  // enables gate-based counters
#define IOPCNT_MODE_GATE (3 << 1)    // 0x6  Gate mode (dependant on counter)
#define IOPCNT_MODE_RESET (1 << 3)   // 0x8  resets the counter on target (if interrupt only?)
#define IOPCNT_INT_TARGET (1 << 4)   // 0x10  triggers an interrupt on targets
#define IOPCNT_INT_OVERFLOW (1 << 5) // 0x20  triggers an interrupt on overflows
#define IOPCNT_INT_TOGGLE (1 << 7)   // 0x80  0=Pulse (reset on read), 1=toggle each interrupt condition (in 1 shot not reset after fired)
#define IOPCNT_ALT_SOURCE (1 << 8)   // 0x100 uses hblank on counters 1 and 3, and PSXCLOCK on counter 0
#define IOPCNT_INT_REQ (1 << 10)     // 0x400 1=Can fire interrupt, 0=Interrupt Fired (reset on read if not 1 shot)

// Use an arbitrary value to flag HBLANK counters.
// These counters will be counted by the hblank gates coming from the EE,
// which ensures they stay 100% in sync with the EE's hblank counters.
#define PSXHBLANK 0x2001

static void psxRcntReset(int index)
{
	psxCounters[index].count = 0;
	psxCounters[index].mode &= ~0x18301C00;
	psxCounters[index].sCycleT = psxRegs.cycle;
}

static void _rcntSet(int cntidx)
{
	u64 overflowCap = (cntidx >= 3) ? 0x100000000ULL : 0x10000;
	u64 c;

	const psxCounter& counter = psxCounters[cntidx];

	// psxNextCounter is relative to the psxRegs.cycle when rcntUpdate() was last called.
	// However, the current _rcntSet could be called at any cycle count, so we need to take
	// that into account.  Adding the difference from that cycle count to the current one
	// will do the trick!

	if (counter.mode & IOPCNT_STOPPED || counter.rate == PSXHBLANK)
		return;

	// check for special cases where the overflow or target has just passed
	// (we probably missed it because we're doing/checking other things)
	if (counter.count > overflowCap || counter.count > counter.target)
	{
		psxNextCounter = 4;
		return;
	}

	c = (u64)((overflowCap - counter.count) * counter.rate) - (psxRegs.cycle - counter.sCycleT);
	c += psxRegs.cycle - psxNextsCounter; // adjust for time passed since last rcntUpdate();

	if (c < (u64)psxNextCounter)
	{
		psxNextCounter = (u32)c;
		psxSetNextBranch(psxNextsCounter, psxNextCounter); //Need to update on counter resets/target changes
	}

	//if((counter.mode & 0x10) == 0 || psxCounters[i].target > 0xffff) continue;
	if (counter.target & IOPCNT_FUTURE_TARGET)
		return;

	c = (s64)((counter.target - counter.count) * counter.rate) - (psxRegs.cycle - counter.sCycleT);
	c += psxRegs.cycle - psxNextsCounter; // adjust for time passed since last rcntUpdate();

	if (c < (u64)psxNextCounter)
	{
		psxNextCounter = (u32)c;
		psxSetNextBranch(psxNextsCounter, psxNextCounter); //Need to update on counter resets/target changes
	}
}


void psxRcntInit()
{
	int i;

	memzero(psxCounters);

	for (i = 0; i < 3; i++)
	{
		psxCounters[i].rate = 1;
		psxCounters[i].mode |= 0x0400;
		psxCounters[i].target = IOPCNT_FUTURE_TARGET;
	}
	for (i = 3; i < 6; i++)
	{
		psxCounters[i].rate = 1;
		psxCounters[i].mode |= 0x0400;
		psxCounters[i].target = IOPCNT_FUTURE_TARGET;
	}

	psxCounters[0].interrupt = 0x10;
	psxCounters[1].interrupt = 0x20;
	psxCounters[2].interrupt = 0x40;

	psxCounters[3].interrupt = 0x04000;
	psxCounters[4].interrupt = 0x08000;
	psxCounters[5].interrupt = 0x10000;

	psxCounters[6].rate = 768 * 12;
	psxCounters[6].CycleT = psxCounters[6].rate;
	psxCounters[6].mode = 0x8;

	if (USBasync != NULL)
	{
		psxCounters[7].rate = PSXCLK / 1000;
		psxCounters[7].CycleT = psxCounters[7].rate;
		psxCounters[7].mode = 0x8;
	}

	for (i = 0; i < 8; i++)
		psxCounters[i].sCycleT = psxRegs.cycle;

	// Tell the IOP to branch ASAP, so that timers can get
	// configured properly.
	psxNextCounter = 1;
	psxNextsCounter = psxRegs.cycle;
}

static bool _rcntFireInterrupt(int i, bool isOverflow)
{
	bool ret;

	if (psxCounters[i].mode & 0x400) //IRQ fired
	{
		psxHu32(0x1070) |= psxCounters[i].interrupt;
		iopTestIntc();
		ret = true;
	}
	else
	{
		ret = false;
		if (!(psxCounters[i].mode & 0x40)) //One shot
			return ret;
	}

	if (psxCounters[i].mode & 0x80) // Toggle mode
		psxCounters[i].mode ^= 0x400; // Interrupt flag inverted
	else
		psxCounters[i].mode &= ~0x0400; // Interrupt flag set low

	return ret;
}
static void _rcntTestTarget(int i)
{
	if (psxCounters[i].count < psxCounters[i].target)
		return;

	if (psxCounters[i].mode & IOPCNT_INT_TARGET)
	{
		// Target interrupt

		if (_rcntFireInterrupt(i, false))
			psxCounters[i].mode |= 0x0800; // Target flag
	}

	if (psxCounters[i].mode & 0x08)
	{
		// Reset on target
		psxCounters[i].count -= psxCounters[i].target;
	}
	else
		psxCounters[i].target |= IOPCNT_FUTURE_TARGET;
}


static __fi void _rcntTestOverflow(int i)
{
	u64 maxTarget = (i < 3) ? 0xffff : 0xfffffffful;
	if (psxCounters[i].count <= maxTarget)
		return;

	if (!(psxCounters[i].mode & 0x40)) //One shot, whichever condition is met first
	{
		if (psxCounters[i].target < IOPCNT_FUTURE_TARGET)
		{ //Target didn't trigger so we can overflow
			// Overflow interrupt
			if ((psxCounters[i].mode & IOPCNT_INT_OVERFLOW))
			{
				if (_rcntFireInterrupt(i, true))
					psxCounters[i].mode |= 0x1000; // Overflow flag
			}
		}
		psxCounters[i].target |= IOPCNT_FUTURE_TARGET;
	}
	else
	{
		// Overflow interrupt
		if ((psxCounters[i].mode & IOPCNT_INT_OVERFLOW))
		{
			if (_rcntFireInterrupt(i, true))
				psxCounters[i].mode |= 0x1000; // Overflow flag
		}
		psxCounters[i].target &= maxTarget;
	}

	// Update count.
	// Count wraps around back to zero, while the target is restored (if not in one shot mode).
	// (high bit of the target gets set by rcntWtarget when the target is behind
	// the counter value, and thus should not be flagged until after an overflow)

	psxCounters[i].count -= maxTarget;
}

/*
Gate:
   TM_NO_GATE                   000
   TM_GATE_ON_Count             001
   TM_GATE_ON_ClearStart        011
   TM_GATE_ON_Clear_OFF_Start   101
   TM_GATE_ON_Start             111

   V-blank  ----+    +----------------------------+    +------
                |    |                            |    |
                |    |                            |    |
                +----+                            +----+
 TM_NO_GATE:

                0================================>============

 TM_GATE_ON_Count:

                <---->0==========================><---->0=====

 TM_GATE_ON_ClearStart:

                0====>0================================>0=====

 TM_GATE_ON_Clear_OFF_Start:

                0====><-------------------------->0====><-----

 TM_GATE_ON_Start:

                <---->0==========================>============
*/

static void _psxCheckStartGate(int i)
{
	if (!(psxCounters[i].mode & IOPCNT_ENABLE_GATE))
		return; //Ignore Gate

	switch ((psxCounters[i].mode & 0x6) >> 1)
	{
		case 0x0: //GATE_ON_count - stop count on gate start:

			// get the current count at the time of stoppage:
			psxCounters[i].count = (i < 3) ?
									   psxRcntRcount16(i) :
									   psxRcntRcount32(i);
			psxCounters[i].mode |= IOPCNT_STOPPED;
			return;

		case 0x1: //GATE_ON_ClearStart - count normally with resets after every end gate
				  // do nothing - All counting will be done on a need-to-count basis.
			return;

		case 0x2: //GATE_ON_Clear_OFF_Start - start counting on gate start, stop on gate end
			psxCounters[i].count = 0;
			psxCounters[i].sCycleT = psxRegs.cycle;
			psxCounters[i].mode &= ~IOPCNT_STOPPED;
			break;

		case 0x3: //GATE_ON_Start - start and count normally on gate end (no restarts or stops or clears)
				  // do nothing!
			return;
	}
	_rcntSet(i);
}

void _psxCheckEndGate(int i)
{
	if (!(psxCounters[i].mode & IOPCNT_ENABLE_GATE))
		return; //Ignore Gate

	switch ((psxCounters[i].mode & 0x6) >> 1)
	{
		case 0x0: //GATE_ON_count - reset and start counting
		case 0x1: //GATE_ON_ClearStart - count normally with resets after every end gate
			psxCounters[i].count = 0;
			psxCounters[i].sCycleT = psxRegs.cycle;
			psxCounters[i].mode &= ~IOPCNT_STOPPED;
			break;

		case 0x2: //GATE_ON_Clear_OFF_Start - start counting on gate start, stop on gate end
			psxCounters[i].count = (i < 3) ?
									   psxRcntRcount16(i) :
									   psxRcntRcount32(i);
			psxCounters[i].mode |= IOPCNT_STOPPED;
			return; // do not set the counter

		case 0x3: //GATE_ON_Start - start and count normally (no restarts or stops or clears)
			if (psxCounters[i].mode & IOPCNT_STOPPED)
			{
				psxCounters[i].count = 0;
				psxCounters[i].sCycleT = psxRegs.cycle;
				psxCounters[i].mode &= ~IOPCNT_STOPPED;
			}
			break;
	}
	_rcntSet(i);
}

/* i always has to be lower than 3 */
void psxCheckStartGate16(int i)
{
	if (i == 0) // hSync counting...
	{
		// AlternateSource/scanline counters for Gates 1 and 3.
		// We count them here so that they stay nicely synced with the EE's hsync.

		const u32 altSourceCheck = IOPCNT_ALT_SOURCE | IOPCNT_ENABLE_GATE;
		const u32 stoppedGateCheck = (IOPCNT_STOPPED | altSourceCheck);

		// count if alt source is enabled and either:
		//  * the gate is enabled and not stopped.
		//  * the gate is disabled.

		if ((psxCounters[1].mode & altSourceCheck) == IOPCNT_ALT_SOURCE ||
			(psxCounters[1].mode & stoppedGateCheck) == altSourceCheck)
		{
			psxCounters[1].count++;
			_rcntTestTarget(1);
			_rcntTestOverflow(1);
		}

		if ((psxCounters[3].mode & altSourceCheck) == IOPCNT_ALT_SOURCE ||
			(psxCounters[3].mode & stoppedGateCheck) == altSourceCheck)
		{
			psxCounters[3].count++;
			_rcntTestTarget(3);
			_rcntTestOverflow(3);
		}
	}

	_psxCheckStartGate(i);
}

void psxVBlankStart(void)
{
	cdvdVsync();
	iopIntcIrq(0);
	if (psxvblankgate & (1 << 1))
		psxCheckStartGate16(1);
	if (psxvblankgate & (1 << 3))
		_psxCheckStartGate(3);
}

void psxVBlankEnd(void)
{
	iopIntcIrq(11);
	if (psxvblankgate & (1 << 1))
		_psxCheckEndGate(1);
	if (psxvblankgate & (1 << 3))
		_psxCheckEndGate(3);
}

void psxRcntUpdate(void)
{
	int i;
	//u32 change = 0;

	g_iopNextEventCycle = psxRegs.cycle + 32;

	psxNextCounter = 0x7fffffff;
	psxNextsCounter = psxRegs.cycle;

	for (i = 0; i <= 5; i++)
	{
		s32 change = psxRegs.cycle - psxCounters[i].sCycleT;

		// don't count disabled or hblank counters...
		// We can't check the ALTSOURCE flag because the PSXCLOCK source *should*
		// be counted here.

		if (psxCounters[i].mode & IOPCNT_STOPPED)
			continue;

		if ((psxCounters[i].mode & 0x40) && !(psxCounters[i].mode & 0x80))
		{ //Repeat IRQ mode Pulsed, resets a few cycles after the interrupt, this should do.
			psxCounters[i].mode |= 0x400;
		}

		if (psxCounters[i].rate == PSXHBLANK)
			continue;

		if (change <= 0)
			continue;

		psxCounters[i].count += change / psxCounters[i].rate;
		if (psxCounters[i].rate != 1)
		{
			change -= (change / psxCounters[i].rate) * psxCounters[i].rate;
			psxCounters[i].sCycleT = psxRegs.cycle - change;
		}
		else
			psxCounters[i].sCycleT = psxRegs.cycle;
	}

	// Do target/overflow testing
	// Optimization Note: This approach is very sound.  Please do not try to unroll it
	// as the size of the Test functions will cause code cache clutter and slowness.

	for (i = 0; i < 6; i++)
	{
		// don't do target/oveflow checks for hblankers.  Those
		// checks are done when the counters are updated.
		if (psxCounters[i].rate == PSXHBLANK)
			continue;
		if (psxCounters[i].mode & IOPCNT_STOPPED)
			continue;

		_rcntTestTarget(i);
		_rcntTestOverflow(i);

		// perform second target test because if we overflowed above it's possible we
		// already shot past our target if it was very near zero.

		//if( psxCounters[i].count >= psxCounters[i].target ) _rcntTestTarget( i );
	}


	const s32 difference = psxRegs.cycle - psxCounters[6].sCycleT;
	s32 c = psxCounters[6].CycleT;

	if (difference >= psxCounters[6].CycleT)
	{
		SPU2async(difference);
		psxCounters[6].sCycleT = psxRegs.cycle;
		psxCounters[6].CycleT = psxCounters[6].rate;
	}
	else
		c -= difference;
	psxNextCounter = c;
	DEV9async(1);
	{
		const s32 difference = psxRegs.cycle - psxCounters[7].sCycleT;
		s32 c = psxCounters[7].CycleT;

		if (difference >= psxCounters[7].CycleT)
		{
			USBasync(difference);
			psxCounters[7].sCycleT = psxRegs.cycle;
			psxCounters[7].CycleT = psxCounters[7].rate;
		}
		else
			c -= difference;
		if (c < psxNextCounter)
			psxNextCounter = c;
	}

	for (i = 0; i < 6; i++)
		_rcntSet(i);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
/* index always has to be lower than 3 */
void psxRcntWcount16(int index, u16 value)
{
	u32 change;

	if (psxCounters[index].rate != PSXHBLANK)
	{
		// Re-adjust the sCycleT to match where the counter is currently
		// (remainder of the rate divided into the time passed will do the trick)

		change = psxRegs.cycle - psxCounters[index].sCycleT;
		psxCounters[index].sCycleT = psxRegs.cycle - (change % psxCounters[index].rate);
	}

	psxCounters[index].count = value & 0xffff;
	if ((psxCounters[index].mode & 0x400) || (psxCounters[index].mode & 0x40))
	{
		psxCounters[index].target &= 0xffff;
	}
	if (value > psxCounters[index].target)
	{   //Count already higher than Target
		psxCounters[index].target |= IOPCNT_FUTURE_TARGET;
	}
	_rcntSet(index);
}

//////////////////////////////////////////////////////////////////////////////////////////
//

/* index always has to be greater than or equal to 3 but lower than 6 */
void psxRcntWcount32(int index, u32 value)
{
	u32 change;

	if (psxCounters[index].rate != PSXHBLANK)
	{
		// Re-adjust the sCycleT to match where the counter is currently
		// (remainder of the rate divided into the time passed will do the trick)

		change = psxRegs.cycle - psxCounters[index].sCycleT;
		psxCounters[index].sCycleT = psxRegs.cycle - (change % psxCounters[index].rate);
	}

	psxCounters[index].count = value;
	if ((psxCounters[index].mode & 0x400) || (psxCounters[index].mode & 0x40))
	{ //IRQ not triggered (one shot) or toggle
		psxCounters[index].target &= 0xffffffff;
	}
	if (value > psxCounters[index].target)
	{ //Count already higher than Target
		psxCounters[index].target |= IOPCNT_FUTURE_TARGET;
	}
	_rcntSet(index);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
__fi void psxRcntWmode16(int index, u32 value)
{
	int irqmode = 0;

	psxCounter& counter = psxCounters[index];

	counter.mode = value;
	counter.mode |= 0x0400; //IRQ Enable

	if (value & (1 << 4))
		irqmode += 1;
	if (value & (1 << 5))
		irqmode += 2;
	if (index == 2)
	{
		switch (value & 0x200)
		{
			case 0x000:
				psxCounters[2].rate = 1;
				break;
			case 0x200:
				psxCounters[2].rate = 8;
				break;
			default:
				break;
		}

		if ((counter.mode & 0x7) == 0x7 || (counter.mode & 0x7) == 0x1)
			counter.mode |= IOPCNT_STOPPED;
	}
	else
	{
		// Counters 0 and 1 can select PIXEL or HSYNC as an alternate source:
		counter.rate = 1;

		if (value & IOPCNT_ALT_SOURCE)
			counter.rate = (index == 0) ? PSXPIXEL : PSXHBLANK;

		if (counter.mode & IOPCNT_ENABLE_GATE)
		{
			// gated counters are added up as per the h/vblank timers.
			// (the PIXEL alt source becomes a vsync gate)
			counter.mode |= IOPCNT_STOPPED;
			if (index == 0)
				psxhblankgate |= 1; // fixme: these gate flags should be one var >_<
			else
				psxvblankgate |= 1 << 1;
		}
		else
		{
			if (index == 0)
				psxhblankgate &= ~1;
			else
				psxvblankgate &= ~(1 << 1);
		}
	}

	counter.count = 0;
	counter.sCycleT = psxRegs.cycle;

	counter.target &= 0xffff;

	_rcntSet(index);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
__fi void psxRcntWmode32(int index, u32 value)
{
	int irqmode = 0;
	psxCounter& counter = psxCounters[index];

	counter.mode = value;
	counter.mode |= 0x0400; //IRQ enable

	if (value & (1 << 4))
		irqmode += 1;
	if (value & (1 << 5))
		irqmode += 2;

	if (index == 3)
	{
		// Counter 3 has the HBlank as an alternate source.
		counter.rate = 1;
		if (value & IOPCNT_ALT_SOURCE)
			counter.rate = PSXHBLANK;

		if (counter.mode & IOPCNT_ENABLE_GATE)
		{
			counter.mode |= IOPCNT_STOPPED;
			psxvblankgate |= 1 << 3;
		}
		else
			psxvblankgate &= ~(1 << 3);
	}
	else
	{
		switch (value & 0x6000)
		{
			case 0x0000:
				counter.rate = 1;
				break;
			case 0x2000:
				counter.rate = 8;
				break;
			case 0x4000:
				counter.rate = 16;
				break;
			case 0x6000:
				counter.rate = 256;
				break;
		}

		// Need to set a rate and target
		if ((counter.mode & 0x7) == 0x7 || (counter.mode & 0x7) == 0x1)
			counter.mode |= IOPCNT_STOPPED;
	}

	counter.count = 0;
	counter.sCycleT = psxRegs.cycle;
	counter.target &= 0xffffffff;
	_rcntSet(index);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
/* index always has to be lower than 3 */
void psxRcntWtarget16(int index, u32 value)
{
	psxCounters[index].target = value & 0xffff;

	// protect the target from an early arrival.
	// if the target is behind the current count, then set the target overflow
	// flag, so that the target won't be active until after the next overflow.

	if (psxCounters[index].target <= psxRcntCycles(index) || ((psxCounters[index].mode & 0x400) == 0 && !(psxCounters[index].mode & 0x40)))
		psxCounters[index].target |= IOPCNT_FUTURE_TARGET;

	_rcntSet(index);
}

/* index always has to be greater than or equal to 3 but lower than 6 */
void psxRcntWtarget32(int index, u32 value)
{
	psxCounters[index].target = value;
	if (!(psxCounters[index].mode & 0x80)) // Toggle mode
		psxCounters[index].mode |= 0x0400; // Interrupt flag set low
	// protect the target from an early arrival.
	// if the target is behind the current count, then set the target overflow
	// flag, so that the target won't be active until after the next overflow.

	if (psxCounters[index].target <= psxRcntCycles(index) || ((psxCounters[index].mode & 0x400) == 0 && !(psxCounters[index].mode & 0x40)))
		psxCounters[index].target |= IOPCNT_FUTURE_TARGET;

	_rcntSet(index);
}

/* index always has to be lower than 3 */
u16 psxRcntRcount16(int index)
{
	u32 retval = (u32)psxCounters[index].count;

	// Don't count HBLANK timers
	// Don't count stopped gates either.

	if (!(psxCounters[index].mode & IOPCNT_STOPPED) &&
		(psxCounters[index].rate != PSXHBLANK))
	{
		u32 delta = (u32)((psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate);
		retval += delta;
	}

	return (u16)retval;
}

/* index always has to be greater than or eqaul to 3 but lower than 6 */
u32 psxRcntRcount32(int index)
{
	u32 retval = (u32)psxCounters[index].count;

	if (!(psxCounters[index].mode & IOPCNT_STOPPED) &&
		(psxCounters[index].rate != PSXHBLANK))
	{
		u32 delta = (u32)((psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate);
		retval += delta;
	}

	return retval;
}

u64 psxRcntCycles(int index)
{
	if (psxCounters[index].mode & IOPCNT_STOPPED || psxCounters[index].rate == PSXHBLANK)
		return psxCounters[index].count;
	return (u64)(psxCounters[index].count + (u32)((psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate));
}

void psxRcntSetGates()
{
	if (psxCounters[0].mode & IOPCNT_ENABLE_GATE)
		psxhblankgate |= 1;
	else
		psxhblankgate &= ~1;

	if (psxCounters[1].mode & IOPCNT_ENABLE_GATE)
		psxvblankgate |= 1 << 1;
	else
		psxvblankgate &= ~(1 << 1);

	if (psxCounters[3].mode & IOPCNT_ENABLE_GATE)
		psxvblankgate |= 1 << 3;
	else
		psxvblankgate &= ~(1 << 3);
}

void SaveStateBase::psxRcntFreeze()
{
	FreezeTag("iopCounters");

	Freeze(psxCounters);
	Freeze(psxNextCounter);
	Freeze(psxNextsCounter);

	if (IsLoading())
		psxRcntSetGates();
}
