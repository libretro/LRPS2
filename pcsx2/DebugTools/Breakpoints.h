// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <vector>

#include "DebugInterface.h"
#include "Pcsx2Types.h"

struct BreakPointCond
{
	DebugInterface *debug;
	PostfixExpression expression;

	BreakPointCond() : debug(NULL)
	{
	}

	u32 Evaluate()
	{
		u64 result;
		if (!debug->parseExpression(expression,result) || result == 0) return 0;
		return 1;
	}
};

struct BreakPoint
{
	BreakPoint() : addr(0), enabled(false), temporary(false), hasCond(false)
	{}

	u32	addr;
	bool enabled;
	bool temporary;

	bool hasCond;
	BreakPointCond cond;

	bool operator == (const BreakPoint &other) const {
		return addr == other.addr;
	}
	bool operator < (const BreakPoint &other) const {
		return addr < other.addr;
	}
};

enum MemCheckCondition
{
	MEMCHECK_READ 		= 0x01,
	MEMCHECK_WRITE 		= 0x02,
	MEMCHECK_WRITE_ONCHANGE = 0x04,
	MEMCHECK_READWRITE 	= 0x03
};

enum MemCheckResult
{
	MEMCHECK_IGNORE 	= 0x00,
	MEMCHECK_LOG 		= 0x01,
	MEMCHECK_BREAK 		= 0x02,
	MEMCHECK_BOTH 		= 0x03
};

struct MemCheck
{
	MemCheck();
	u32 start;
	u32 end;

	MemCheckCondition cond;
	MemCheckResult result;

	u32 numHits;

	u32 lastPC;
	u32 lastAddr;
	int lastSize;

	bool operator == (const MemCheck &other) const {
		return start == other.start && end == other.end;
	}
};

// BreakPoints cannot overlap, only one is allowed per address.
// MemChecks can overlap, as long as their ends are different.
// WARNING: MemChecks are not used in the interpreter or HLE currently.
class CBreakPoints
{
public:
	static const size_t INVALID_BREAKPOINT = -1;
	static const size_t INVALID_MEMCHECK = -1;

	static bool IsAddressBreakPoint(u32 addr);
	static bool IsAddressBreakPoint(u32 addr, bool* enabled);

	// Makes a copy.  Temporary breakpoints can't have conditions.
	static BreakPointCond *GetBreakPointCondition(u32 addr);

	static u32 CheckSkipFirst(u32 pc);

	// Includes uncached addresses.
	static const std::vector<MemCheck> GetMemChecks();
	static size_t GetNumMemchecks() { return memChecks_.size(); }

	static void Update(u32 addr = 0);

	static void SetBreakpointTriggered(bool b) { breakpointTriggered_ = b; };

private:
	static size_t FindBreakpoint(u32 addr, bool matchTemp = false, bool temp = false);

	static std::vector<BreakPoint> breakPoints_;
	static u32 breakSkipFirstAt_;
	static u64 breakSkipFirstTicks_;
	static bool breakpointTriggered_;

	static std::vector<MemCheck> memChecks_;
};

// called from the dynarec
u32 __fastcall standardizeBreakpointAddress(u32 addr);
