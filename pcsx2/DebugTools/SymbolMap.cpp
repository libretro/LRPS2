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

#include "Pcsx2Defs.h"
#include "SymbolMap.h"

#include <algorithm>
#include <cstring> /* strncpy, strchr, strcmp */

SymbolMap symbolMap;

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

void SymbolMap::Clear() {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	functions.clear();
	labels.clear();
	data.clear();
	activeFunctions.clear();
	activeLabels.clear();
	activeData.clear();
	activeModuleEnds.clear();
	modules.clear();
}


bool SymbolMap::LoadNocashSym(const char *filename) {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	FILE *f = fopen(filename, "r");
	if (!f)
		return false;

	while (!feof(f)) {
		char line[256], value[256] = {0};
		char *p = fgets(line, 256, f);
		if (p == NULL)
			break;

		u32 address;
		if (sscanf(line, "%08X %s", &address, value) != 2)
			continue;
		if (address == 0 && strcmp(value, "0") == 0)
			continue;

		if (value[0] == '.') {
			// data directives
			char* s = strchr(value, ':');
			if (s != NULL) {
				*s = 0;

				u32 size = 0;
				if (sscanf(s + 1, "%04X", &size) != 1)
					continue;

				if (strcasecmp(value, ".byt") == 0) {
					AddData(address, size, DATATYPE_BYTE, 0);
				} else if (strcasecmp(value, ".wrd") == 0) {
					AddData(address, size, DATATYPE_HALFWORD, 0);
				} else if (strcasecmp(value, ".dbl") == 0) {
					AddData(address, size, DATATYPE_WORD, 0);
				} else if (strcasecmp(value, ".asc") == 0) {
					AddData(address, size, DATATYPE_ASCII, 0);
				}
			}
		} else {				// labels
			int size = 1;
			char* seperator = strchr(value, ',');
			if (seperator != NULL) {
				*seperator = 0;
				sscanf(seperator+1,"%08X",&size);
			}

			if (size != 1) {
				AddFunction(value, address,size, 0);
			} else {
				AddLabel(value, address, 0);
			}
		}
	}

	fclose(f);
	return true;
}

bool SymbolMap::GetSymbolInfo(SymbolInfo *info, u32 address, SymbolType symmask) const {
	u32 functionAddress = INVALID_ADDRESS;
	u32 dataAddress     = INVALID_ADDRESS;

	if (symmask & ST_FUNCTION)
		functionAddress = GetFunctionStart(address);

	if (symmask & ST_DATA)
		dataAddress = GetDataStart(address);

	if (functionAddress == INVALID_ADDRESS || dataAddress == INVALID_ADDRESS) {
		if (functionAddress != INVALID_ADDRESS) {
			if (info != NULL) {
				info->type = ST_FUNCTION;
				info->address = functionAddress;
				info->size = GetFunctionSize(functionAddress);
			}

			return true;
		}
		
		if (dataAddress != INVALID_ADDRESS) {
			if (info != NULL) {
				info->type = ST_DATA;
				info->address = dataAddress;
				info->size = GetDataSize(dataAddress);
			}

			return true;
		}

		return false;
	}

	// if both exist, return the function
	if (info != NULL) {
		info->type = ST_FUNCTION;
		info->address = functionAddress;
		info->size = GetFunctionSize(functionAddress);
	}

	return true;
}

u32 SymbolMap::GetModuleRelativeAddr(u32 address, int moduleIndex) const {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	if (moduleIndex == -1) {
		moduleIndex = GetModuleIndex(address);
	}

	for (auto it = modules.begin(), end = modules.end(); it != end; ++it) {
		if (it->index == moduleIndex) {
			return address - it->start;
		}
	}
	return address;
}

int SymbolMap::GetModuleIndex(u32 address) const {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto iter = activeModuleEnds.upper_bound(address);
	if (iter == activeModuleEnds.end())
		return -1;
	return iter->second.index;
}

bool SymbolMap::IsModuleActive(int moduleIndex) const {
	if (moduleIndex == 0) {
		return true;
	}

	std::lock_guard<std::recursive_mutex> guard(m_lock);
	for (auto it = activeModuleEnds.begin(), end = activeModuleEnds.end(); it != end; ++it) {
		if (it->second.index == moduleIndex) {
			return true;
		}
	}
	return false;
}

void SymbolMap::AddFunction(const char* name, u32 address, u32 size, int moduleIndex) {
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	if (moduleIndex == -1)
		moduleIndex = GetModuleIndex(address);

	// Is there an existing one?
	u32 relAddress = GetModuleRelativeAddr(address, moduleIndex);
	auto symbolKey = std::make_pair(moduleIndex, relAddress);
	auto existing = functions.find(symbolKey);
	if (existing == functions.end()) {
		// Fall back: maybe it's got moduleIndex = 0.
		existing = functions.find(std::make_pair(0, address));
	}

	if (existing != functions.end()) {
		existing->second.size = size;
		if (existing->second.module != moduleIndex) {
			existing->second.start = relAddress;
			existing->second.module = moduleIndex;
		}

		// Refresh the active item if it exists.
		auto active = activeFunctions.find(address);
		if (active != activeFunctions.end() && active->second.module == moduleIndex) {
			activeFunctions.erase(active);
			activeFunctions.insert(std::make_pair(address, existing->second));
		}
	} else {
		FunctionEntry func;
		func.start = relAddress;
		func.size = size;
		func.index = (int)functions.size();
		func.module = moduleIndex;
		functions[symbolKey] = func;

		if (IsModuleActive(moduleIndex)) {
			activeFunctions.insert(std::make_pair(address, func));
		}
	}

	AddLabel(name, address, moduleIndex);
}

u32 SymbolMap::GetFunctionStart(u32 address) const {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = activeFunctions.upper_bound(address);
	if (it == activeFunctions.end()) {
		// check last element
		auto rit = activeFunctions.rbegin();
		if (rit != activeFunctions.rend()) {
			u32 start = rit->first;
			u32 size = rit->second.size;
			if (start <= address && start+size > address)
				return start;
		}
		// otherwise there's no function that contains this address
		return INVALID_ADDRESS;
	}

	if (it != activeFunctions.begin()) {
		it--;
		u32 start = it->first;
		u32 size = it->second.size;
		if (start <= address && start+size > address)
			return start;
	}

	return INVALID_ADDRESS;
}

u32 SymbolMap::GetFunctionSize(u32 startAddress) const {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = activeFunctions.find(startAddress);
	if (it == activeFunctions.end())
		return INVALID_ADDRESS;

	return it->second.size;
}

void SymbolMap::AssignFunctionIndices() {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	int index = 0;
	for (auto mod = activeModuleEnds.begin(), modend = activeModuleEnds.end(); mod != modend; ++mod) {
		int moduleIndex = mod->second.index;
		auto begin = functions.lower_bound(std::make_pair(moduleIndex, 0));
		auto end = functions.upper_bound(std::make_pair(moduleIndex, 0xFFFFFFFF));
		for (auto it = begin; it != end; ++it) {
			it->second.index = index++;
		}
	}
}

void SymbolMap::UpdateActiveSymbols() {
	// return;   (slow in debug mode)
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	std::map<int, u32> activeModuleIndexes;
	for (auto it = activeModuleEnds.begin(), end = activeModuleEnds.end(); it != end; ++it) {
		activeModuleIndexes[it->second.index] = it->second.start;
	}

	activeFunctions.clear();
	activeLabels.clear();
	activeData.clear();

	for (auto it = functions.begin(), end = functions.end(); it != end; ++it) {
		const auto mod = activeModuleIndexes.find(it->second.module);
		if (it->second.module <= 0) {
			activeFunctions.insert(std::make_pair(it->second.start, it->second));
		} else if (mod != activeModuleIndexes.end()) {
			activeFunctions.insert(std::make_pair(mod->second + it->second.start, it->second));
		}
	}

	for (auto it = labels.begin(), end = labels.end(); it != end; ++it) {
		const auto mod = activeModuleIndexes.find(it->second.module);
		if (it->second.module <= 0) {
			activeLabels.insert(std::make_pair(it->second.addr, it->second));
		} else if (mod != activeModuleIndexes.end()) {
			activeLabels.insert(std::make_pair(mod->second + it->second.addr, it->second));
		}
	}

	for (auto it = data.begin(), end = data.end(); it != end; ++it) {
		const auto mod = activeModuleIndexes.find(it->second.module);
		if (it->second.module <= 0) {
			activeData.insert(std::make_pair(it->second.start, it->second));
		} else if (mod != activeModuleIndexes.end()) {
			activeData.insert(std::make_pair(mod->second + it->second.start, it->second));
		}
	}

	AssignFunctionIndices();
}

void SymbolMap::AddLabel(const char* name, u32 address, int moduleIndex) {
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	if (moduleIndex == -1) {
		moduleIndex = GetModuleIndex(address);
	}

	// Is there an existing one?
	u32 relAddress = GetModuleRelativeAddr(address, moduleIndex);
	auto symbolKey = std::make_pair(moduleIndex, relAddress);
	auto existing = labels.find(symbolKey);
	if (existing == labels.end()) {
		// Fall back: maybe it's got moduleIndex = 0.
		existing = labels.find(std::make_pair(0, address));
	}

	if (existing != labels.end()) {
		// We leave an existing label alone, rather than overwriting.
		// But we'll still upgrade it to the correct module / relative address.
		if (existing->second.module != moduleIndex) {
			existing->second.addr = relAddress;
			existing->second.module = moduleIndex;

			// Refresh the active item if it exists.
			auto active = activeLabels.find(address);
			if (active != activeLabels.end() && active->second.module == moduleIndex) {
				activeLabels.erase(active);
				activeLabels.insert(std::make_pair(address, existing->second));
			}
		}
	} else {
		LabelEntry label;
		label.addr = relAddress;
		label.module = moduleIndex;
		strncpy(label.name, name, 128);
		label.name[127] = 0;

		labels[symbolKey] = label;
		if (IsModuleActive(moduleIndex)) {
			activeLabels.insert(std::make_pair(address, label));
		}
	}
}

bool SymbolMap::GetLabelValue(const char* name, u32& dest) {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	for (auto it = activeLabels.begin(); it != activeLabels.end(); it++) {
		if (strcasecmp(name, it->second.name) == 0) {
			dest = it->first;
			return true;
		}
	}

	return false;
}

void SymbolMap::AddData(u32 address, u32 size, DataType type, int moduleIndex) {
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	if (moduleIndex == -1) {
		moduleIndex = GetModuleIndex(address);
	}

	// Is there an existing one?
	u32 relAddress = GetModuleRelativeAddr(address, moduleIndex);
	auto symbolKey = std::make_pair(moduleIndex, relAddress);
	auto existing = data.find(symbolKey);
	if (existing == data.end()) {
		// Fall back: maybe it's got moduleIndex = 0.
		existing = data.find(std::make_pair(0, address));
	}

	if (existing != data.end()) {
		existing->second.size = size;
		existing->second.type = type;
		if (existing->second.module != moduleIndex) {
			existing->second.module = moduleIndex;
			existing->second.start = relAddress;
		}

		// Refresh the active item if it exists.
		auto active = activeData.find(address);
		if (active != activeData.end() && active->second.module == moduleIndex) {
			activeData.erase(active);
			activeData.insert(std::make_pair(address, existing->second));
		}
	} else {
		DataEntry entry;
		entry.start = relAddress;
		entry.size = size;
		entry.type = type;
		entry.module = moduleIndex;

		data[symbolKey] = entry;
		if (IsModuleActive(moduleIndex)) {
			activeData.insert(std::make_pair(address, entry));
		}
	}
}

u32 SymbolMap::GetDataStart(u32 address) const {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = activeData.upper_bound(address);
	if (it == activeData.end())
	{
		// check last element
		auto rit = activeData.rbegin();

		if (rit != activeData.rend())
		{
			u32 start = rit->first;
			u32 size = rit->second.size;
			if (start <= address && start+size > address)
				return start;
		}
		// otherwise there's no data that contains this address
		return INVALID_ADDRESS;
	}

	if (it != activeData.begin()) {
		it--;
		u32 start = it->first;
		u32 size = it->second.size;
		if (start <= address && start+size > address)
			return start;
	}

	return INVALID_ADDRESS;
}

u32 SymbolMap::GetDataSize(u32 startAddress) const {
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = activeData.find(startAddress);
	if (it == activeData.end())
		return INVALID_ADDRESS;
	return it->second.size;
}
