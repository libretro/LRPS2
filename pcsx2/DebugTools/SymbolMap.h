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

#include <vector>
#include <set>
#include <map>
#include <string>
#include <mutex>

#include "Pcsx2Types.h"

enum SymbolType {
	ST_NONE     = 0,
	ST_FUNCTION = 1,
	ST_DATA     = 2,
	ST_ALL      = 3
};

struct SymbolInfo {
	SymbolType type;
	u32 address;
	u32 size;
};

struct SymbolEntry {
	std::string name;
	u32 address;
	u32 size;
};

struct LoadedModuleInfo {
	std::string name;
	u32 address;
	u32 size;
	bool active;
};

enum DataType {
	DATATYPE_NONE, DATATYPE_BYTE, DATATYPE_HALFWORD, DATATYPE_WORD, DATATYPE_ASCII
};

class SymbolMap {
public:
	SymbolMap() {}
	void Clear();

	bool LoadNocashSym(const char *ilename);

	bool GetSymbolInfo(SymbolInfo *info, u32 address, SymbolType symmask = ST_FUNCTION) const;

	u32 GetModuleRelativeAddr(u32 address, int moduleIndex = -1) const;
	int GetModuleIndex(u32 address) const;
	bool IsModuleActive(int moduleIndex) const;

	void AddFunction(const char* name, u32 address, u32 size, int moduleIndex = -1);
	u32 GetFunctionStart(u32 address) const;
	u32 GetFunctionSize(u32 startAddress) const;

	void AddLabel(const char* name, u32 address, int moduleIndex = -1);
	bool GetLabelValue(const char* name, u32& dest);

	void AddData(u32 address, u32 size, DataType type, int moduleIndex = -1);
	u32 GetDataStart(u32 address) const;
	u32 GetDataSize(u32 startAddress) const;

	static const u32 INVALID_ADDRESS = (u32)-1;

	void UpdateActiveSymbols();
	bool IsEmpty() const { return activeFunctions.empty() && activeLabels.empty() && activeData.empty(); };
private:
	void AssignFunctionIndices();

	struct FunctionEntry {
		u32 start;
		u32 size;
		int index;
		int module;
	};

	struct LabelEntry {
		u32 addr;
		int module;
		char name[128];
	};

	struct DataEntry {
		DataType type;
		u32 start;
		u32 size;
		int module;
	};

	struct ModuleEntry {
		// Note: this index is +1, 0 matches any for backwards-compat.
		int index;
		u32 start;
		u32 size;
		char name[128];
	};

	// These are flattened, read-only copies of the actual data in active modules only.
	std::map<u32, const FunctionEntry> activeFunctions;
	std::map<u32, const LabelEntry> activeLabels;
	std::map<u32, const DataEntry> activeData;

	// This is indexed by the end address of the module.
	std::map<u32, const ModuleEntry> activeModuleEnds;

	typedef std::pair<int, u32> SymbolKey;

	// These are indexed by the module id and relative address in the module.
	std::map<SymbolKey, FunctionEntry> functions;
	std::map<SymbolKey, LabelEntry> labels;
	std::map<SymbolKey, DataEntry> data;
	std::vector<ModuleEntry> modules;

	mutable std::recursive_mutex m_lock;
};

extern SymbolMap symbolMap;

