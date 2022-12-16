/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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
#include "wx/wx.h"

#include "DebugInterface.h"
#include "Memory.h"
#include "R5900.h"
#include "Debug.h"
#include "../VU.h"
#include "Counters.h"

#include "../R3000A.h"
#include "../IopMem.h"
#include "SymbolMap.h"

R5900DebugInterface r5900Debug;

//
// R5900DebugInterface
//

u32 R5900DebugInterface::read8(u32 address)
{
	if (!isValidAddress(address))
		return -1;

	return memRead8(address);
}

u32 R5900DebugInterface::read16(u32 address)
{
	if (!isValidAddress(address) || address % 2)
		return -1;

	return memRead16(address);
}

u32 R5900DebugInterface::read32(u32 address)
{
	if (!isValidAddress(address) || address % 4)
		return -1;

	return memRead32(address);
}

u64 R5900DebugInterface::read64(u32 address)
{
	if (!isValidAddress(address) || address % 8)
		return -1;

	u64 result;
	memRead64(address,result);
	return result;
}

u128 R5900DebugInterface::read128(u32 address)
{
	__aligned16 u128 result;
	if (!isValidAddress(address) || address % 16)
	{
		result.hi = result.lo = -1;
		return result;
	}

	memRead128(address,result);
	return result;
}

void R5900DebugInterface::write8(u32 address, u8 value)
{
	if (!isValidAddress(address))
		return;

	memWrite8(address,value);
}

void R5900DebugInterface::write32(u32 address, u32 value)
{
	if (!isValidAddress(address))
		return;

	memWrite32(address,value);
}


const char* R5900DebugInterface::getRegisterName(int cat, int num)
{
	switch (cat)
	{
	case EECAT_GPR:
		switch (num)
		{
		case 32:	// pc
			return "pc";
		case 33:	// hi
			return "hi";
		case 34:	// lo
			return "lo";
		default:
			return R5900::GPR_REG[num];
		}
	case EECAT_CP0:
		return R5900::COP0_REG[num];
	case EECAT_FPR:
		return R5900::COP1_REG_FP[num];
	case EECAT_FCR:
		return R5900::COP1_REG_FCR[num];
	case EECAT_VU0F:
		switch (num)
		{
		case 32:	// ACC
			return "ACC";
		default:
			return R5900::COP2_REG_FP[num];
		}
	case EECAT_VU0I:
		return R5900::COP2_REG_CTL[num];
	default:
		break;
	}

	return "Invalid";
}

u128 R5900DebugInterface::getRegister(int cat, int num)
{
	u128 result;
	switch (cat)
	{
	case EECAT_GPR:
		switch (num)
		{
		case 32:	// pc
			result = u128::From32(cpuRegs.pc);
			break;
		case 33:	// hi
			result = cpuRegs.HI.UQ;
			break;
		case 34:	// lo
			result = cpuRegs.LO.UQ;
			break;
		default:
			result = cpuRegs.GPR.r[num].UQ;
			break;
		}
		break;
	case EECAT_CP0:
		result = u128::From32(cpuRegs.CP0.r[num]);
		break;
	case EECAT_FPR:
		result = u128::From32(fpuRegs.fpr[num].UL);
		break;
	case EECAT_FCR:
		result = u128::From32(fpuRegs.fprc[num]);
		break;
	case EECAT_VU0F:
		switch (num)
		{
		case 32:	// ACC
			result = VU0.ACC.UQ;
			break;
		default:
			result = VU0.VF[num].UQ;
			break;
		}
		break;
	case EECAT_VU0I:
		result = u128::From32(VU0.VI[num].UL);
		break;
	default:
		result = u128::From32(0);
		break;
	}

	return result;
}

u128 R5900DebugInterface::getHI()
{
	return cpuRegs.HI.UQ;
}

u128 R5900DebugInterface::getLO()
{
	return cpuRegs.LO.UQ;
}

u32 R5900DebugInterface::getPC()
{
	return cpuRegs.pc;
}

bool R5900DebugInterface::isValidAddress(u32 addr)
{
	u32 lopart = addr & 0xfFFffFF;

	// get rid of ee ram mirrors
	switch (addr >> 28)
	{
	case 0:
	case 2:
		// case 3: throw exception (not mapped ?)
		// [ 0000_8000 - 01FF_FFFF ] RAM
		// [ 2000_8000 - 21FF_FFFF ] RAM MIRROR
		// [ 3000_8000 - 31FF_FFFF ] RAM MIRROR
		if (lopart >= 0x80000 && lopart <= 0x1ffFFff)
			return !!vtlb_GetPhyPtr(lopart);
		break;
	case 1:
		// [ 1000_0000 - 1000_CFFF ] EE register
		if (lopart <= 0xcfff)
			return true;

		// [ 1100_0000 - 1100_FFFF ] VU mem
		if (lopart >= 0x1000000 && lopart <= 0x100FFff)
			return true;

		// [ 1200_0000 - 1200_FFFF ] GS regs
		if (lopart >= 0x2000000 && lopart <= 0x20010ff)
			return true;

		// [ 1E00_0000 - 1FFF_FFFF ] ROM
		// if (lopart >= 0xe000000)
		// 	return true; throw exception (not mapped ?)
		break;
	case 7:
		// [ 7000_0000 - 7000_3FFF ] Scratchpad
		if (lopart <= 0x3fff)
			return true;
		break;
	case 8:
	case 9:
	case 0xA:
	case 0xB:
		// [ 8000_0000 - BFFF_FFFF ] kernel
		// We only need to access the EE kernel (which is 1 MB large)
		if (lopart < 0x100000)
			return true;
		break;
	case 0xF:
		// [ 8000_0000 - BFFF_FFFF ] IOP or kernel stack
		if (lopart >= 0xfff8000)
			return true;
		break;
	}

	return false;
}
