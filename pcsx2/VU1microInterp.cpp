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
#include "MTVU.h"

extern void _vuFlushAll(VURegs* VU);

void _vu1ExecUpper(VURegs* VU, u32 *ptr) {
	VU->code = ptr[1];
	VU1_UPPER_OPCODE[VU->code & 0x3f]();
}

void _vu1ExecLower(VURegs* VU, u32 *ptr) {
	VU->code = ptr[0];
	VU1_LOWER_OPCODE[VU->code >> 25]();
}

int vu1branch = 0;

static void _vu1Exec(VURegs* VU)
{
	_VURegsNum lregs;
	_VURegsNum uregs;
	VECTOR _VF;
	VECTOR _VFc;
	REG_VI _VI;
	REG_VI _VIc;
	u32 *ptr;
	int vfreg;
	int vireg;
	int discard=0;

	ptr = (u32*)&VU->Micro[VU->VI[REG_TPC].UL];
	VU->VI[REG_TPC].UL+=8;

	if (ptr[1] & 0x40000000) { /* E flag */
		VU->ebit = 2;
	}
	if (ptr[1] & 0x10000000) { /* D flag */
		if (VU0.VI[REG_FBRST].UL & 0x400) {
			VU0.VI[REG_VPU_STAT].UL|= 0x200;
			hwIntcIrq(INTC_VU1);
			VU->ebit = 1;
		}
		
	}
	if (ptr[1] & 0x08000000) { /* T flag */
		if (VU0.VI[REG_FBRST].UL & 0x800) {
			VU0.VI[REG_VPU_STAT].UL|= 0x400;
			hwIntcIrq(INTC_VU1);
			VU->ebit = 1;
		}
		
	}

	VU->code = ptr[1];
	VU1regs_UPPER_OPCODE[VU->code & 0x3f](&uregs);
#ifndef INT_VUSTALLHACK
	_vuTestUpperStalls(VU, &uregs);
#endif

	/* check upper flags */
	if (ptr[1] & 0x80000000) { /* I flag */
		_vu1ExecUpper(VU, ptr);

		VU->VI[REG_I].UL = ptr[0];
		//Lower not used, set to 0 to fill in the FMAC stall gap
		//Could probably get away with just running upper stalls, but lets not tempt fate.
		memset(&lregs, 0, sizeof(lregs));		
	} else {
		VU->code = ptr[0];
		VU1regs_LOWER_OPCODE[VU->code >> 25](&lregs);
#ifndef INT_VUSTALLHACK
		_vuTestLowerStalls(VU, &lregs);
#endif

		vu1branch = lregs.pipe == VUPIPE_BRANCH;

		vfreg = 0; vireg = 0;
		if (uregs.VFwrite) {
			if (lregs.VFwrite == uregs.VFwrite) {
//				log_cb(RETRO_LOG_WARN, "*PCSX2*: Warning, VF write to the same reg in both lower/upper cycle\n");
				discard = 1;
			}
			if (lregs.VFread0 == uregs.VFwrite ||
				lregs.VFread1 == uregs.VFwrite) {
//				log_cb(RETRO_LOG_INFO, "saving reg %d at pc=%x\n", i, VU->VI[REG_TPC].UL);
				_VF = VU->VF[uregs.VFwrite];
				vfreg = uregs.VFwrite;
			}
		}
		if (uregs.VIread & (1 << REG_CLIP_FLAG)) {
			if (lregs.VIwrite & (1 << REG_CLIP_FLAG)) {
				log_cb(RETRO_LOG_WARN, "*PCSX2*: Warning, VI write to the same reg in both lower/upper cycle\n");
				discard = 1;
			}
			if (lregs.VIread & (1 << REG_CLIP_FLAG)) {
				_VI = VU->VI[REG_CLIP_FLAG];
				vireg = REG_CLIP_FLAG;
			}
		}

		_vu1ExecUpper(VU, ptr);

		if (discard == 0) {
			if (vfreg) {
				_VFc = VU->VF[vfreg];
				VU->VF[vfreg] = _VF;
			}
			if (vireg) {
				_VIc = VU->VI[vireg];
				VU->VI[vireg] = _VI;
			}

			_vu1ExecLower(VU, ptr);

			if (vfreg) {
				VU->VF[vfreg] = _VFc;
			}
			if (vireg) {
				VU->VI[vireg] = _VIc;
			}
		}
	}
	_vuAddUpperStalls(VU, &uregs);
	_vuAddLowerStalls(VU, &lregs);

	_vuTestPipes(VU);
	
	if(VU->VIBackupCycles > 0) 
		VU->VIBackupCycles--;
	
	if (VU->branch > 0) {
		if (VU->branch-- == 1) {
			VU->VI[REG_TPC].UL = VU->branchpc;

			if(VU->takedelaybranch)
			{				
				VU->branch = 1;
				//log_cb(RETRO_LOG_DEBUG, "VU1 - Branch/Jump in Delay Slot\n");			
				VU->branchpc = VU->delaybranchpc;
				VU->delaybranchpc = 0;
				VU->takedelaybranch = false;
			}			
		}
	}

	if( VU->ebit > 0 ) {
		if( VU->ebit-- == 1 ) {
			VU->VIBackupCycles = 0;
			_vuFlushAll(VU);
			VU0.VI[REG_VPU_STAT].UL &= ~0x100;
			vif1Regs.stat.VEW = false;
		}
	}
}

void vu1Exec(VURegs* VU)
{
	_vu1Exec(VU);
	VU->cycle++;

#if 0
	if (VU->VI[0].UL != 0)
		log_cb(RETRO_LOG_DEBUG, "VI[0] != 0!!!!\n");
	if (VU->VF[0].f.x != 0.0f)
		log_cb(RETRO_LOG_DEBUG, "VF[0].x != 0.0!!!!\n");
	if (VU->VF[0].f.y != 0.0f)
		log_cb(RETRO_LOG_DEBUG, "VF[0].y != 0.0!!!!\n");
	if (VU->VF[0].f.z != 0.0f)
		log_cb(RETRO_LOG_DEBUG, "VF[0].z != 0.0!!!!\n");
	if (VU->VF[0].f.w != 1.0f)
		log_cb(RETRO_LOG_DEBUG, "VF[0].w != 1.0!!!!\n");
#endif
}

InterpVU1::InterpVU1()
{
	m_Idx = 1;
	IsInterpreter = true;
}

void InterpVU1::Reset() {
	vu1Thread.WaitVU();
}

void InterpVU1::Shutdown() noexcept {
	vu1Thread.WaitVU();
}

void InterpVU1::SetStartPC(u32 startPC)
{
	VU1.start_pc = startPC;
}

void InterpVU1::Step()
{
	VU1.VI[REG_TPC].UL &= VU1_PROGMASK;
	vu1Exec( &VU1 );
}

void InterpVU1::Execute(u32 cycles)
{
	VU1.VI[REG_TPC].UL <<= 3;
	for (int i = (int)cycles; i > 0; i--) {
		if (!(VU0.VI[REG_VPU_STAT].UL & 0x100)) {
			if (VU1.branch || VU1.ebit) {
				Step(); // run branch delay slot?
			}
			break;
		}
		Step();
	}
	VU1.VI[REG_TPC].UL >>= 3;
}

