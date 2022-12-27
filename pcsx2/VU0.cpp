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


/* TODO
 -Fix the flags Proper as they aren't handle now..
 -Add BC Table opcodes
 -Add Interlock in QMFC2,QMTC2,CFC2,CTC2
 -Finish instruction set
 -Bug Fixes!!!
*/

#include "PrecompiledHeader.h"
#include "Common.h"

#include <cmath>

#include "R5900OpcodeTables.h"
#include "VUmicro.h"
#include "Vif_Dma.h"

#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

#define _Fsf_ ((cpuRegs.code >> 21) & 0x03)
#define _Ftf_ ((cpuRegs.code >> 23) & 0x03)

using namespace R5900;

void COP2_BC2(void) { Int_COP2BC2PrintTable[_Rt_]();}
void COP2_SPECIAL(void) { Int_COP2SPECIAL1PrintTable[_Funct_]();}

void COP2_SPECIAL2(void) {
	Int_COP2SPECIAL2PrintTable[(cpuRegs.code & 0x3) | ((cpuRegs.code >> 4) & 0x7c)]();
}

void COP2_Unknown(void) { }

//****************************************************************************

static __fi void _vu0run(bool breakOnMbit, bool addCycles)
{
	if (!(VU0.VI[REG_VPU_STAT].UL & 1)) return;

	//VU0 is ahead of the EE and M-Bit is already encountered, so no need to wait for it, just catch up the EE
	if ((VU0.flags & VUFLAG_MFLAGSET) && breakOnMbit && VU0.cycle >= cpuRegs.cycle)
	{
		cpuRegs.cycle = VU0.cycle;
		return;
	}

	u32 startcycle = cpuRegs.cycle;
	u32 runCycles  = 0x7fffffff;

	do { // Run VU until it finishes or M-Bit
		CpuVU0->Execute(runCycles);
	} while ((VU0.VI[REG_VPU_STAT].UL & 1)						// E-bit Termination
	  &&	(!breakOnMbit || !(VU0.flags & VUFLAG_MFLAGSET) || VU0.cycle < cpuRegs.cycle));	// M-bit Break

	// Add cycles if called from EE's COP2
	if (addCycles)
	{
		cpuRegs.cycle += (VU0.cycle - startcycle);
	}
}

void _vu0WaitMicro(void)   { _vu0run(1, 1); } // Runs VU0 Micro Until E-bit or M-Bit End
void _vu0FinishMicro(void) { _vu0run(0, 1); } // Runs VU0 Micro Until E-Bit End
void vu0Finish(void)	   { _vu0run(0, 0); } // Runs VU0 Micro Until E-Bit End (doesn't stall EE)

namespace R5900 {
namespace Interpreter{
namespace OpcodeImpl
{
	void LQC2(void) {
		u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + (s16)cpuRegs.code;
		if (_Ft_) {
			memRead128(addr, VU0.VF[_Ft_].UQ);
		} else {
			u128 val;
 			memRead128(addr, val);
		}
	}

	// Asadr.Changed
	//TODO: check this
	// HUH why ? doesn't make any sense ...
	void SQC2() {
		u32 addr = _Imm_ + cpuRegs.GPR.r[_Rs_].UL[0];
		memWrite128(addr, VU0.VF[_Ft_].UQ);
	}
}}}


void QMFC2(void)
{
	if (cpuRegs.code & 1)
		_vu0FinishMicro();
	if (_Rt_ == 0) return;
	cpuRegs.GPR.r[_Rt_].UD[0] = VU0.VF[_Fs_].UD[0];
	cpuRegs.GPR.r[_Rt_].UD[1] = VU0.VF[_Fs_].UD[1];
}

void QMTC2(void)
{
	if (cpuRegs.code & 1)
		_vu0WaitMicro();
	if (_Fs_ == 0) return;
	VU0.VF[_Fs_].UD[0] = cpuRegs.GPR.r[_Rt_].UD[0];
	VU0.VF[_Fs_].UD[1] = cpuRegs.GPR.r[_Rt_].UD[1];
}

void CFC2(void)
{
	if (cpuRegs.code & 1)
		_vu0FinishMicro();
	if (_Rt_ == 0) return;
	
	cpuRegs.GPR.r[_Rt_].UL[0] = VU0.VI[_Fs_].UL;

	if(VU0.VI[_Fs_].UL & 0x80000000)
		cpuRegs.GPR.r[_Rt_].UL[1] = 0xffffffff;
	else
		cpuRegs.GPR.r[_Rt_].UL[1] = 0;
}

void CTC2(void)
{
	if (cpuRegs.code & 1)
		_vu0WaitMicro();
	if (_Fs_ == 0) return;

	switch(_Fs_) {
		case REG_MAC_FLAG: // read-only
		case REG_TPC:      // read-only
		case REG_VPU_STAT: // read-only
			break;
		case REG_R:
			VU0.VI[REG_R].UL = ((cpuRegs.GPR.r[_Rt_].UL[0] & 0x7FFFFF) | 0x3F800000);
			break;
		case REG_FBRST:
			VU0.VI[REG_FBRST].UL = cpuRegs.GPR.r[_Rt_].UL[0] & 0x0C0C;
			if (cpuRegs.GPR.r[_Rt_].UL[0] & 0x2) { // VU0 Reset
				vu0ResetRegs();
			}
			if (cpuRegs.GPR.r[_Rt_].UL[0] & 0x200) { // VU1 Reset
				vu1ResetRegs();
			}
			break;
		case REG_CMSAR1: // REG_CMSAR1
			vu1Finish(true);
			vu1ExecMicro(cpuRegs.GPR.r[_Rt_].US[0]);	// Execute VU1 Micro SubRoutine
			break;
		default:
			VU0.VI[_Fs_].UL = cpuRegs.GPR.r[_Rt_].UL[0];
			break;
	}
}
