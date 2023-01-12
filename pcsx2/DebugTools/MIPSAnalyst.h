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

#pragma once

#include "Pcsx2Types.h"

class DebugInterface;

#define MIPS_GET_OP(op)   ((op>>26) & 0x3F)
#define MIPS_GET_FUNC(op) (op & 0x3F)
#define MIPS_GET_SA(op)   ((op>>6) & 0x1F)

#define MIPS_GET_RS(op) ((op>>21) & 0x1F)
#define MIPS_GET_RT(op) ((op>>16) & 0x1F)
#define MIPS_GET_RD(op) ((op>>11) & 0x1F)

namespace MIPSAnalyst
{
	struct AnalyzedFunction {
		u32 start;
		u32 end;
		u32 size;
	};

	void ScanForFunctions(u32 startAddr, u32 endAddr, bool insertSymbols);
};
