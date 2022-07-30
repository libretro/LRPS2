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

#include "VUmicro.h"
#include "GS.h"
#include "Gif_Unit.h"
#include "MTVU.h"

#include <cfenv>

extern void _vuFlushAll(VURegs* VU);

static void _vu1ExecUpper(VURegs* VU, u32 *ptr)
{
	VU->code = ptr[1];
	VU1_UPPER_OPCODE[VU->code & 0x3f]();
}

static void _vu1ExecLower(VURegs* VU, u32 *ptr)
{
	VU->code = ptr[0];
	VU1_LOWER_OPCODE[VU->code >> 25]();
}

static void _vu1Exec(VURegs* VU)
{
	_VURegsNum lregs;
	_VURegsNum uregs;
	VECTOR _VF;
	VECTOR _VFc;
	REG_VI _VI;
	REG_VI _VIc;
	int vfreg;
	int vireg;
	int discard=0;
	u32 *ptr = (u32*)&VU->Micro[VU->VI[REG_TPC].UL];
	VU->VI[REG_TPC].UL+=8;

	if (ptr[1] & 0x40000000) /* E flag */
		VU->ebit = 2;
	if (ptr[1] & 0x10000000) /* D flag */
	{ 
		if (VU0.VI[REG_FBRST].UL & 0x400)
		{
			VU0.VI[REG_VPU_STAT].UL|= 0x200;
			hwIntcIrq(INTC_VU1);
			VU->ebit = 1;
		}
	}
	if (ptr[1] & 0x08000000) /* T flag */
	{
		if (VU0.VI[REG_FBRST].UL & 0x800)
		{
			VU0.VI[REG_VPU_STAT].UL|= 0x400;
			hwIntcIrq(INTC_VU1);
			VU->ebit = 1;
		}
	}

	VU->code = ptr[1];
	VU1regs_UPPER_OPCODE[VU->code & 0x3f](&uregs);
	_vuTestUpperStalls(VU, &uregs);

	/* check upper flags */
	if (ptr[1] & 0x80000000) /* I flag */
	{
		_vuTestPipes(VU);
		_vu1ExecUpper(VU, ptr);

		VU->VI[REG_I].UL = ptr[0];
		//Lower not used, set to 0 to fill in the FMAC stall gap
		//Could probably get away with just running upper stalls, but lets not tempt fate.
		memset(&lregs, 0, sizeof(lregs));		
	}
	else
	{
		VU->code = ptr[0];
		VU1regs_LOWER_OPCODE[VU->code >> 25](&lregs);
		_vuTestLowerStalls(VU, &lregs);
		_vuTestPipes(VU);

		vfreg = 0;
		vireg = 0;
		if (uregs.VFwrite)
		{
			if (lregs.VFwrite == uregs.VFwrite)
				discard = 1;
			if (   lregs.VFread0 == uregs.VFwrite 
			    || lregs.VFread1 == uregs.VFwrite)
			{
				_VF = VU->VF[uregs.VFwrite];
				vfreg = uregs.VFwrite;
			}
		}
		if (uregs.VIwrite & (1 << REG_CLIP_FLAG))
		{
			if (lregs.VIwrite & (1 << REG_CLIP_FLAG))
				discard = 1;
			if (lregs.VIread & (1 << REG_CLIP_FLAG))
			{
				_VI = VU->VI[REG_CLIP_FLAG];
				vireg = REG_CLIP_FLAG;
			}
		}

		_vu1ExecUpper(VU, ptr);

		if (discard == 0)
		{
			if (vfreg)
			{
				_VFc = VU->VF[vfreg];
				VU->VF[vfreg] = _VF;
			}
			if (vireg)
			{
				_VIc = VU->VI[vireg];
				VU->VI[vireg] = _VI;
			}

			_vu1ExecLower(VU, ptr);

			if (vfreg)
				VU->VF[vfreg] = _VFc;
			if (vireg)
				VU->VI[vireg] = _VIc;
		}
	}
	_vuAddUpperStalls(VU, &uregs);
	_vuAddLowerStalls(VU, &lregs);

	if(VU->VIBackupCycles > 0) 
		VU->VIBackupCycles--;
	
	if (VU->branch > 0)
	{
		if (VU->branch-- == 1)
		{
			VU->VI[REG_TPC].UL = VU->branchpc;

			if (VU->takedelaybranch)
			{				
				VU->branch          = 1;
				VU->branchpc        = VU->delaybranchpc;
				VU->delaybranchpc   = 0;
				VU->takedelaybranch = false;
			}			
		}
	}

	if( VU->ebit > 0 )
	{
		if( VU->ebit-- == 1 )
		{
			VU->VIBackupCycles = 0;
			_vuFlushAll(VU);
			VU0.VI[REG_VPU_STAT].UL &= ~0x100;
			vif1Regs.stat.VEW = false;
		}
	}
	if (VU->xgkickenable && (VU1.cycle - VU->xgkicklastcycle) >= 2)
	{
		if (VU->xgkicksizeremaining == 0)
		{
			u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, VU->Mem, VU->xgkickaddr);
			VU->xgkicksizeremaining = size & 0xFFFF;
			VU->xgkickendpacket = size >> 31;
			if (VU->xgkicksizeremaining == 0)
				VU->xgkickenable = 0;
		}
		u32 transfersize = std::min(VU->xgkicksizeremaining / 0x10, (VU1.cycle - VU->xgkicklastcycle) / 2);
		transfersize = std::min(transfersize, VU->xgkickdiff / 0x10);

		if (transfersize)
		{
			if ((transfersize * 0x10) > VU->xgkicksizeremaining)
				gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&VU->Mem[VU->xgkickaddr], transfersize * 0x10, true);
			else
				gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &VU->Mem[VU->xgkickaddr], transfersize * 0x10, true);

			VU->xgkickaddr = (VU->xgkickaddr + (transfersize * 0x10)) & 0x3FFF;
			VU->xgkicksizeremaining -= (transfersize * 0x10);
			VU->xgkickdiff = 0x4000 - VU->xgkickaddr;
			VU->xgkicklastcycle += std::max(transfersize * 2, 2U);

			if (VU->xgkicksizeremaining || !VU->xgkickendpacket)
				VU->xgkickenable = 1;
			else
				VU->xgkickenable = 0;
		}
	}
}

static void vu1Exec(VURegs* VU)
{
	VU->cycle++;
	_vu1Exec(VU);
}

InterpVU1::InterpVU1()
{
	m_Idx = 1;
}

void InterpVU1::Reset()
{
	vu1Thread.WaitVU();
}

void InterpVU1::Shutdown() noexcept
{
	vu1Thread.WaitVU();
}

void InterpVU1::SetStartPC(u32 startPC)
{
	VU1.start_pc = startPC;
}

void InterpVU1::Execute(u32 cycles)
{
	const int originalRounding = fegetround();
	fesetround(g_sseVUMXCSR.RoundingControl << 8);

	VU1.VI[REG_TPC].UL <<= 3;
	for (int i = (int)cycles; i > 0; i--)
	{
		if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
		{
			/* run branch delay slot? */
			if (VU1.branch || VU1.ebit)
			{
				VU1.VI[REG_TPC].UL &= VU1_PROGMASK;
				vu1Exec( &VU1 );
			}
			break;
		}
		VU1.VI[REG_TPC].UL &= VU1_PROGMASK;
		vu1Exec(&VU1);
	}
	VU1.VI[REG_TPC].UL >>= 3;

	fesetround(originalRounding);
}

