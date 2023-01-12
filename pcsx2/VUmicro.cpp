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
#include "VUmicro.h"
#include "MTVU.h"

// Executes a Block based on EE delta time
void BaseVUmicroCPU::ExecuteBlock(bool startUp) {
	const u32& stat	= VU0.VI[REG_VPU_STAT].UL;
	const int  test = m_Idx ? 0x100 : 1;
	const int s = EmuConfig.Gamefixes.VUKickstartHack ? 16 : 0; // Kick Start Cycles (Jak needs at least 4 due to writing values after they're read

	if (m_Idx && THREAD_VU1)
	{
		vu1Thread.Get_GSChanges();
	}

	if (!(stat & test)) return;

	if (startUp && s) {  // Start Executing a microprogram
		Execute(s); // Kick start VU
	}
	else { // Continue Executing
		u32 cycle = m_Idx ? VU1.cycle : VU0.cycle;
		s32 delta = (s32)(u32)(cpuRegs.cycle - cycle);
		s32 nextblockcycles = m_Idx ? VU1.nextBlockCycles : VU0.nextBlockCycles;

		if (delta >= nextblockcycles) // Enough time has passed
			Execute(delta);	// Execute the time since the last call
	}
}

// This function is called by VU0 Macro (COP2) after transferring some
// EE data to VU0's registers. We want to run VU0 Micro right after this
// to ensure that the register is used at the correct time.
// This fixes spinning/hanging in some games like Ratchet and Clank's Intro.
void BaseVUmicroCPU::ExecuteBlockJIT(BaseVUmicroCPU* cpu) {
	const u32& stat	= VU0.VI[REG_VPU_STAT].UL;
	const int  test = 1;

	if (stat & test) {		// VU is running
		s32 delta = (s32)(u32)(cpuRegs.cycle - VU0.cycle);
		s32 nextblockcycles = VU0.nextBlockCycles;

		if (delta < nextblockcycles)
			return;

		if (delta > 0) {			// Enough time has passed
			cpu->Execute(delta);	// Execute the time since the last call
		}
	}
}
