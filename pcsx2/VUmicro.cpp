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

#include "PrecompiledHeader.h"
#include "Common.h"
#include "Gif_Unit.h"
#include "VUmicro.h"
#include "MTVU.h"

// Executes a Block based on EE delta time
#if 1
void BaseVUmicroCPU::ExecuteBlock(bool startUp) {
	const u32& stat	= VU0.VI[REG_VPU_STAT].UL;
	const int  test = m_Idx ? 0x100 : 1;
	const int s = EmuConfig.Gamefixes.VUKickstartHack ? 16 : 0; // Kick Start Cycles (Jak needs at least 4 due to writing values after they're read

	if (m_Idx && THREAD_VU1)
	{
		vu1Thread.Get_GSChanges();
	}

	if (!(stat & test))
	{
		if (m_Idx == 1)
		{
			if (VU1.xgkickenable && (cpuRegs.cycle - VU1.xgkicklastcycle) >= 2)
			{
				if (VU1.xgkicksizeremaining == 0)
				{
					u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, VU1.Mem, VU1.xgkickaddr);
					VU1.xgkicksizeremaining = size & 0xFFFF;
					VU1.xgkickendpacket = size >> 31;
				}
				u32 transfersize = std::min(VU1.xgkicksizeremaining / 0x10, (cpuRegs.cycle - VU1.xgkicklastcycle) / 2);
				transfersize = std::min(transfersize, VU1.xgkickdiff / 0x10);

				if (transfersize)
				{
					if ((transfersize * 0x10) > VU1.xgkicksizeremaining)
						gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&VU1.Mem[VU1.xgkickaddr], transfersize * 0x10, true);
					else
						gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &VU1.Mem[VU1.xgkickaddr], transfersize * 0x10, true);

					VU1.xgkickaddr = (VU1.xgkickaddr + (transfersize * 0x10)) & 0x3FFF;
					VU1.xgkicksizeremaining -= (transfersize * 0x10);
					VU1.xgkickdiff = 0x4000 - VU1.xgkickaddr;
					VU1.xgkicklastcycle += std::max(transfersize * 2, 2U);

					if (VU1.xgkicksizeremaining || !VU1.xgkickendpacket)
						VU1.xgkickenable = 1;
					else
						VU1.xgkickenable = 0;
				}
				else
					VU1.xgkickenable = 1;
			}
		}
		return;
	}

	if (startUp && s)  // Start Executing a microprogram
		Execute(s); // Kick start VU
	else               // Continue Executing
	{ 
		u32 cycle = m_Idx ? VU1.cycle : VU0.cycle;
		s32 delta = (s32)(u32)(cpuRegs.cycle - cycle);
		s32 nextblockcycles = m_Idx ? VU1.nextBlockCycles : VU0.nextBlockCycles;

		if (delta >= nextblockcycles) // Enough time has passed
			Execute(delta);	// Execute the time since the last call
	}
}
#else
/* Should work correctly with Crash Twinsanity? */
void BaseVUmicroCPU::ExecuteBlock(bool startUp)
{
	const u32& stat = VU0.VI[REG_VPU_STAT].UL;
	const int test = m_Idx ? 0x100 : 1;
	const int s = EmuConfig.Gamefixes.VUKickstartHack ? 16 : 0; // Kick Start Cycles (Jak needs at least 4 due to writing values after they're read

	if (m_Idx && THREAD_VU1)
	{
		vu1Thread.Get_GSChanges();
		return;
	}

	if (!(stat & test))
		return;

	if (startUp && s) // Start Executing a microprogram (When kickstarted)
	{
		Execute(s); // Kick start VU

		// I don't like doing this, but Crash Twinsanity seems to be upset without it
		if (stat & test)
		{
			if (m_Idx)
			        cpuRegs.cycle = VU1.cycle;
			else
				cpuRegs.cycle = VU0.cycle;

			cpuSetNextEventDelta(s);
		}
	}
	else // Continue Executing
	{
		u32 cycle = m_Idx ? VU1.cycle : VU0.cycle;
		s32 delta = (s32)(u32)(cpuRegs.cycle - cycle);
		s32 nextblockcycles = m_Idx ? VU1.nextBlockCycles : VU0.nextBlockCycles;

		if (EmuConfig.Gamefixes.VUKickstartHack)
		{
			if (delta > 0)  // When kickstarting we just need 1 cycle for run ahead
			Execute(delta);
		}
		else
		{
			if (delta >= nextblockcycles) // When running behind, make sure we have enough cycles passed for the block to run
				Execute(delta);
		}

		if (stat & test)
		{
			// Queue up next required time to run a block
			nextblockcycles = m_Idx ? VU1.nextBlockCycles : VU0.nextBlockCycles;
			cycle           = m_Idx ? VU1.cycle : VU0.cycle;
			nextblockcycles = EmuConfig.Gamefixes.VUKickstartHack ? (cycle - cpuRegs.cycle) : nextblockcycles;

			if(nextblockcycles)
				cpuSetNextEventDelta(nextblockcycles);
		}
	}
}
#endif

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
