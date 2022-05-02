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
#include "Breakpoints.h"
#include "SymbolMap.h"
#include "MIPSAnalyst.h"
#include <cstdio>
#include "../R5900.h"
#include "../System.h"

std::vector<BreakPoint> CBreakPoints::breakPoints_;
u32 CBreakPoints::breakSkipFirstAt_ = 0;
u64 CBreakPoints::breakSkipFirstTicks_ = 0;
std::vector<MemCheck> CBreakPoints::memChecks_;
bool CBreakPoints::breakpointTriggered_ = false;

// called from the dynarec
u32 __fastcall standardizeBreakpointAddress(u32 addr)
{
	if (addr >= 0xFFFF8000)
		return addr;

	if (addr >= 0xBFC00000 && addr <= 0xBFFFFFFF)
		addr &= 0x1FFFFFFF;

	addr &= 0x7FFFFFFF;
	
	if ((addr >> 28) == 2 || (addr >> 28) == 3)
		addr &= ~(0xF << 28);
	
	return addr;
}

MemCheck::MemCheck() :
	start(0),
	end(0),
	cond(MEMCHECK_READWRITE),
	result(MEMCHECK_BOTH),
	lastPC(0),
	lastAddr(0),
	lastSize(0)
{
	numHits = 0;
}

size_t CBreakPoints::FindBreakpoint(u32 addr, bool matchTemp, bool temp)
{
	addr = standardizeBreakpointAddress(addr);

	for (size_t i = 0; i < breakPoints_.size(); ++i)
	{
		u32 cmp = standardizeBreakpointAddress(breakPoints_[i].addr);
		if (cmp == addr && (!matchTemp || breakPoints_[i].temporary == temp))
			return i;
	}

	return INVALID_BREAKPOINT;
}

bool CBreakPoints::IsAddressBreakPoint(u32 addr)
{
	size_t bp = FindBreakpoint(addr);
	if (bp != INVALID_BREAKPOINT && breakPoints_[bp].enabled)
		return true;
	// Check again for overlapping temp breakpoint
	bp = FindBreakpoint(addr, true, true);
	return bp != INVALID_BREAKPOINT && breakPoints_[bp].enabled;
}

bool CBreakPoints::IsAddressBreakPoint(u32 addr, bool* enabled)
{
	size_t bp = FindBreakpoint(addr);
	if (bp == INVALID_BREAKPOINT) return false;
	if (enabled != NULL) *enabled = breakPoints_[bp].enabled;
	return true;
}

BreakPointCond *CBreakPoints::GetBreakPointCondition(u32 addr)
{
	size_t bp = FindBreakpoint(addr, true, true);
	//temp breakpoints are unconditional
	if (bp != INVALID_BREAKPOINT)
		return NULL;

	bp = FindBreakpoint(addr, true, false);
	if (bp != INVALID_BREAKPOINT && breakPoints_[bp].hasCond)
		return &breakPoints_[bp].cond;
	return NULL;
}

u32 CBreakPoints::CheckSkipFirst(u32 cmpPc)
{
	cmpPc = standardizeBreakpointAddress(cmpPc);
	u32 pc = breakSkipFirstAt_;
	if (breakSkipFirstTicks_ == r5900Debug.getCycles())
		return pc;
	return 0;
}

const std::vector<MemCheck> CBreakPoints::GetMemChecks()
{
	return memChecks_;
}

// including them earlier causes some ambiguities
#include "App.h"
void CBreakPoints::Update(u32 addr)
{
	bool resume = false;
	if (!r5900Debug.isCpuPaused())
	{
		r5900Debug.pauseCpu();
		resume = true;
	}

	SysClearExecutionCache();

	if (resume)
		r5900Debug.resumeCpu();
}
