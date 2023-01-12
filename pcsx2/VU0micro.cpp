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
#include "VUmicro.h"

#include <cmath>

// This is called by the COP2 as per the CTC instruction
void vu0ResetRegs(void)
{
	VU0.VI[REG_VPU_STAT].UL &= ~0xff; // stop vu0
	VU0.VI[REG_FBRST].UL &= ~0xff; // stop vu0
	vif0Regs.stat.VEW = false;
}

void vu0ExecMicro(u32 addr)
{
	/* vu0ExecMicro > Stalling for previous microprogram to finish */
	if(VU0.VI[REG_VPU_STAT].UL & 0x1)
		vu0Finish();

	// Need to copy the clip flag back to the interpreter in case COP2 has edited it
	VU0.clipflag = VU0.VI[REG_CLIP_FLAG].UL;
	VU0.VI[REG_VPU_STAT].UL &= ~0xFF;
	VU0.VI[REG_VPU_STAT].UL |=  0x01;
	VU0.cycle = cpuRegs.cycle;
	if ((s32)addr != -1) VU0.VI[REG_TPC].UL = addr & 0x1FF;

	CpuVU0->SetStartPC(VU0.VI[REG_TPC].UL << 3);
	CpuVU0->ExecuteBlock(1);
}
