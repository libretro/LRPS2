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


// This module contains code shared by both the dynarec and interpreter versions
// of the VU0 micro.

#include "Common.h"
#include <cmath>
#include "VUmicro.h"
#include "MTVU.h"

// This is called by the COP2 as per the CTC instruction
void vu1ResetRegs(void)
{
	VU0.VI[REG_VPU_STAT].UL &= ~0xff00; // stop vu1
	VU0.VI[REG_FBRST].UL    &= ~0xff00; // stop vu1
	vif1Regs.stat.VEW        = false;
}

void vu1Finish(bool add_cycles)
{
	if (THREAD_VU1)
		return;
	u32 vu1cycles = VU1.cycle;
	/* vu1ExecMicro > Stalling until current microprogram finishes */
	if (VU0.VI[REG_VPU_STAT].UL & 0x100)
		CpuVU1->Execute(vu1RunCycles);
	/* Force Stopping VU1, ran for too long */
	if (VU0.VI[REG_VPU_STAT].UL & 0x100)
		VU0.VI[REG_VPU_STAT].UL &= ~0x100;
	if (add_cycles)
		cpuRegs.cycle += VU1.cycle - vu1cycles;
}

void vu1ExecMicro(u32 addr)
{
	if (THREAD_VU1)
	{
		vu1Thread.ExecuteVU(addr, vif1Regs.top, vif1Regs.itop);
		VU0.VI[REG_VPU_STAT].UL &= ~0xFF00;
		return;
	}
	vu1Finish(false);

	VU1.cycle                = cpuRegs.cycle;
	VU0.VI[REG_VPU_STAT].UL &= ~0xFF00;
	VU0.VI[REG_VPU_STAT].UL |=  0x0100;
	if ((s32)addr != -1) VU1.VI[REG_TPC].UL = addr & 0x7FF;

	CpuVU1->SetStartPC(VU1.VI[REG_TPC].UL << 3);
	if (!INSTANT_VU1)
		CpuVU1->ExecuteBlock(1);
	else
		CpuVU1->Execute(vu1RunCycles);
}
