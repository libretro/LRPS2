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

#pragma once

struct EECNT_MODE
{
	// 0 - BUSCLK
	// 1 - 1/16th of BUSCLK
	// 2 - 1/256th of BUSCLK
	// 3 - External Clock (hblank!)
	u32 ClockSource:2;

	// Enables the counter gate (turns counter on/off as according to the
	// h/v blank type set in GateType).
	u32 EnableGate:1;

	// 0 - hblank!  1 - vblank!
	// Note: the hblank source type is disabled when ClockSel = 3
	u32 GateSource:1;

	// 0 - count when the gate signal is low
	// 1 - reset and start counting at the signal's rising edge (h/v blank end)
	// 2 - reset and start counting at the signal's falling edge (h/v blank start)
	// 3 - reset and start counting at both signal edges
	u32 GateMode:2;

	// Counter cleared to zero when target reached.
	// The PS2 only resets if the TargetInterrupt is enabled - Tested on PS2
	u32 ZeroReturn:1;

	// General count enable/status.  If 0, no counting happens.
	// This flag is set/unset by the gates.
	u32 IsCounting:1;

	// Enables target interrupts.
	u32 TargetInterrupt:1;

	// Enables overflow interrupts.
	u32 OverflowInterrupt:1;

	// Set to true by the counter when the target is reached.
	// Flag is set only when TargetInterrupt is enabled.
	u32 TargetReached:1;

	// Set to true by the counter when the target has overflowed.
	// Flag is set only when OverflowInterrupt is enabled.
	u32 OverflowReached:1;
};

// fixme: Cycle and sCycleT members are unused.
//	      But they can't be removed without making a new savestate version.
struct Counter
{
	u32 count;
	union
	{
		u32 modeval;		// the mode as a 32 bit int (for optimized combination masks)
		EECNT_MODE mode;
	};
	u32 target, hold;
	u32 rate, interrupt;
	u32 sCycleT;		// delta values should be signed.
};

struct SyncCounter
{
	u32 Mode;
	u32 sCycle;		// start cycle of timer
	s32 CycleT;
};

extern Counter counters[4];
extern SyncCounter hsyncCounter;
extern SyncCounter vsyncCounter;

extern s32 nextCounter;		// delta until the next counter event (must be signed)
extern u32 nextsCounter;

extern void rcntUpdate_hScanline(void);
extern void rcntUpdate(void);

extern void rcntInit(void);
extern u32	rcntRcount(int index);
template< uint page > extern bool rcntWrite32( u32 mem, mem32_t& value );
template< uint page > extern u16 rcntRead32( u32 mem );		// returns u16 by design! (see implementation for details)

extern void UpdateVSyncRate(void);
