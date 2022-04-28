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

#include <string>
#include <algorithm>
#include <map>

#include "DisassemblyManager.h"
#include "Memory.h"
#include "Debug.h"
#include "MIPSAnalyst.h"

int DisassemblyManager::maxParamChars = 29;

static u32 computeHash(u32 address, u32 size)
{
	u32 end = address+size;
	u32 hash = 0xBACD7814;
	while (address < end)
	{
		hash += memRead32(address);
		address += 4;
	}
	return hash;
}

void DisassemblyManager::clear()
{
	for (auto it = entries.begin(); it != entries.end(); it++)
	{
		delete it->second;
	}
	entries.clear();
}

DisassemblyFunction::DisassemblyFunction(DebugInterface* _cpu, u32 _address, u32 _size): address(_address), size(_size)
{
	cpu = _cpu;
	hash = computeHash(address,size);
	load();
}

int DisassemblyFunction::getNumLines()
{
	return (int) lineAddresses.size();
}

int DisassemblyFunction::getLineNum(u32 address, bool findStart)
{
	if (findStart)
	{
		int last = (int)lineAddresses.size() - 1;
		for (int i = 0; i < last; i++)
		{
			u32 next = lineAddresses[i + 1];
			if (lineAddresses[i] <= address && next > address)
				return i;
		}
		if (lineAddresses[last] <= address && this->address + this->size > address)
			return last;
	}
	else
	{
		int last = (int)lineAddresses.size() - 1;
		for (int i = 0; i < last; i++)
		{
			if (lineAddresses[i] == address)
				return i;
		}
		if (lineAddresses[last] == address)
			return last;
	}

	return 0;
}

u32 DisassemblyFunction::getLineAddress(int line)
{
	return lineAddresses[line];
}

#define NUM_LANES 16

void DisassemblyFunction::generateBranchLines()
{
	struct LaneInfo
	{
		bool used;
		u32 end;
	};

	LaneInfo lanes[NUM_LANES];
	for (int i = 0; i < NUM_LANES; i++) {
		lanes[i].used = false;
		lanes[i].end = 0;
	}

	u32 end = address+size;

	for (u32 funcPos = address; funcPos < end; funcPos += 4)
	{
		MIPSAnalyst::MipsOpcodeInfo opInfo = MIPSAnalyst::GetOpcodeInfo(cpu,funcPos);

		bool inFunction = (opInfo.branchTarget >= address && opInfo.branchTarget < end);
		if (opInfo.isBranch && !opInfo.isBranchToRegister && !opInfo.isLinkedBranch && inFunction)
		{
			BranchLine line;
			if (opInfo.branchTarget < funcPos)
			{
				line.first = opInfo.branchTarget;
				line.second = funcPos;
				line.type = LINE_UP;
			} else {
				line.first = funcPos;
				line.second = opInfo.branchTarget;
				line.type = LINE_DOWN;
			}

			lines.push_back(line);
		}
	}
			
	std::sort(lines.begin(),lines.end());
	for (size_t i = 0; i < lines.size(); i++)
	{
		for (int l = 0; l < NUM_LANES; l++)
		{
			if (lines[i].first > lanes[l].end)
				lanes[l].used = false;
		}

		int lane = -1;
		for (int l = 0; l < NUM_LANES; l++)
		{
			if (!lanes[l].used)
			{
				lane = l;
				break;
			}
		}

		if (lane == -1)
		{
			// error
			continue;
		}

		lanes[lane].end = lines[i].second;
		lanes[lane].used = true;
		lines[i].laneIndex = lane;
	}
}

void DisassemblyFunction::addOpcodeSequence(u32 start, u32 end)
{
	DisassemblyOpcode* opcode = new DisassemblyOpcode(cpu,start,(end-start)/4);
	entries[start] = opcode;
	for (u32 pos = start; pos < end; pos += 4)
	{
		lineAddresses.push_back(pos);
	}
}

void DisassemblyFunction::load()
{
	generateBranchLines();

	// gather all branch targets
	std::set<u32> branchTargets;
	for (size_t i = 0; i < lines.size(); i++)
	{
		switch (lines[i].type)
		{
		case LINE_DOWN:
			branchTargets.insert(lines[i].second);
			break;
		case LINE_UP:
			branchTargets.insert(lines[i].first);
			break;
		}
	}
	
	u32 funcPos = address;
	u32 funcEnd = address+size;

	u32 nextData = symbolMap.GetNextSymbolAddress(funcPos-1,ST_DATA);
	u32 opcodeSequenceStart = funcPos;
	while (funcPos < funcEnd)
	{
		if (funcPos == nextData)
		{
			if (opcodeSequenceStart != funcPos)
				addOpcodeSequence(opcodeSequenceStart,funcPos);

			DisassemblyData* data = new DisassemblyData(cpu,funcPos,symbolMap.GetDataSize(funcPos),symbolMap.GetDataType(funcPos));
			entries[funcPos] = data;
			lineAddresses.push_back(funcPos);
			funcPos += data->getTotalSize();

			nextData = symbolMap.GetNextSymbolAddress(funcPos-1,ST_DATA);
			opcodeSequenceStart = funcPos;
			continue;
		}

		// force align
		if (funcPos % 4)
		{
			u32 nextPos = (funcPos+3) & ~3;

			DisassemblyComment* comment = new DisassemblyComment(cpu,funcPos,nextPos-funcPos,".align","4");
			entries[funcPos] = comment;
			lineAddresses.push_back(funcPos);
			
			funcPos = nextPos;
			opcodeSequenceStart = funcPos;
			continue;
		}

		MIPSAnalyst::MipsOpcodeInfo opInfo = MIPSAnalyst::GetOpcodeInfo(cpu,funcPos);
		u32 opAddress = funcPos;
		funcPos += 4;
		
		// skip branches and their delay slots
		if (opInfo.isBranch)
		{
			if (funcPos < funcEnd) funcPos += 4; // only include delay slots within the function bounds
			continue;
		}
		
		// lui
		if (MIPS_GET_OP(opInfo.encodedOpcode) == 0x0F && funcPos < funcEnd && funcPos != nextData)
		{
			u32 next = cpu->read32(funcPos);

			u32 immediate = ((opInfo.encodedOpcode & 0xFFFF) << 16) + (s16)(next & 0xFFFF);
			u32 immediateOr = ((opInfo.encodedOpcode & 0xFFFF) << 16) | (u16)(next & 0xFFFF);
			int rt = MIPS_GET_RT(opInfo.encodedOpcode);

			int nextRs = MIPS_GET_RS(next);
			int nextRt = MIPS_GET_RT(next);

			// both rs and rt of the second op have to match rt of the first,
			// otherwise there may be hidden consequences if the macro is displayed.
			// also, don't create a macro if something branches into the middle of it
			if (nextRs == rt && nextRt == rt && branchTargets.find(funcPos) == branchTargets.end())
			{
				DisassemblyMacro* macro = NULL;
				switch (MIPS_GET_OP(next))
				{
				case 0x09:	// addiu
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroLi(immediate,rt);
					funcPos += 4;
					break;
				case 0x0D:	// ori
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroLi(immediateOr,rt);
					funcPos += 4;
					break;
				case 0x20:	// lb
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroMemory("lb",immediate,rt,1);
					funcPos += 4;
					break;
				case 0x21:	// lh
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroMemory("lh",immediate,rt,2);
					funcPos += 4;
					break;
				case 0x23:	// lw
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroMemory("lw",immediate,rt,4);
					funcPos += 4;
					break;
				case 0x24:	// lbu
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroMemory("lbu",immediate,rt,1);
					funcPos += 4;
					break;
				case 0x25:	// lhu
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroMemory("lhu",immediate,rt,2);
					funcPos += 4;
					break;
				case 0x28:	// sb
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroMemory("sb",immediate,rt,1);
					funcPos += 4;
					break;
				case 0x29:	// sh
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroMemory("sh",immediate,rt,2);
					funcPos += 4;
					break;
				case 0x2B:	// sw
					macro = new DisassemblyMacro(cpu,opAddress);
					macro->setMacroMemory("sw",immediate,rt,4);
					funcPos += 4;
					break;
				}

				if (macro != NULL)
				{
					if (opcodeSequenceStart != opAddress)
						addOpcodeSequence(opcodeSequenceStart,opAddress);

					entries[opAddress] = macro;
					for (int i = 0; i < macro->getNumLines(); i++)
					{
						lineAddresses.push_back(macro->getLineAddress(i));
					}

					opcodeSequenceStart = funcPos;
					continue;
				}
			}
		}

		// just a normal opcode
	}

	if (opcodeSequenceStart != funcPos)
		addOpcodeSequence(opcodeSequenceStart,funcPos);
}

void DisassemblyFunction::clear()
{
	for (auto it = entries.begin(); it != entries.end(); it++)
	{
		delete it->second;
	}

	entries.clear();
	lines.clear();
	lineAddresses.clear();
	hash = 0;
}


void DisassemblyMacro::setMacroLi(u32 _immediate, u8 _rt)
{
	type = MACRO_LI;
	name = "li";
	immediate = _immediate;
	rt = _rt;
	numOpcodes = 2;
}

void DisassemblyMacro::setMacroMemory(std::string _name, u32 _immediate, u8 _rt, int _dataSize)
{
	type = MACRO_MEMORYIMM;
	name = _name;
	immediate = _immediate;
	rt = _rt;
	dataSize = _dataSize;
	numOpcodes = 2;
}

DisassemblyData::DisassemblyData(DebugInterface* _cpu, u32 _address, u32 _size, DataType _type): address(_address), size(_size), type(_type)
{
	cpu = _cpu;
	hash = computeHash(address,size);
	createLines();
}

int DisassemblyData::getLineNum(u32 address, bool findStart)
{
	auto it = lines.upper_bound(address);
	if (it != lines.end())
	{
		if (it == lines.begin())
			return 0;
		it--;
		return it->second.lineNum;
	}

	return lines.rbegin()->second.lineNum;
}

void DisassemblyData::createLines()
{
	lines.clear();
	lineAddresses.clear();

	u32 pos = address;
	u32 end = address+size;
	u32 maxChars = DisassemblyManager::getMaxParamChars();
	
	std::string currentLine;
	u32 currentLineStart = pos;

	int lineCount = 0;
	if (type == DATATYPE_ASCII)
	{
		bool inString = false;
		while (pos < end)
		{
			u8 b = memRead8(pos++);
			if (b >= 0x20 && b <= 0x7F)
			{
				if (currentLine.size()+1 >= maxChars)
				{
					if (inString)
						currentLine += "\"";

					DataEntry entry = {currentLine,pos-1-currentLineStart,lineCount++};
					lines[currentLineStart] = entry;
					lineAddresses.push_back(currentLineStart);
					
					currentLine = "";
					currentLineStart = pos-1;
					inString = false;
				}

				if (!inString)
					currentLine += "\"";
				currentLine += (char)b;
				inString = true;
			} else {
				char buffer[64];
				if (pos == end && b == 0)
					strcpy(buffer,"0");
				else
					sprintf(buffer,"0x%02X",b);

				if (currentLine.size()+strlen(buffer) >= maxChars)
				{
					if (inString)
						currentLine += "\"";
					
					DataEntry entry = {currentLine,pos-1-currentLineStart,lineCount++};
					lines[currentLineStart] = entry;
					lineAddresses.push_back(currentLineStart);
					
					currentLine = "";
					currentLineStart = pos-1;
					inString = false;
				}

				bool comma = false;
				if (currentLine.size() != 0)
					comma = true;

				if (inString)
					currentLine += "\"";

				if (comma)
					currentLine += ",";

				currentLine += buffer;
				inString = false;
			}
		}

		if (inString)
			currentLine += "\"";

		if (currentLine.size() != 0)
		{
			DataEntry entry = {currentLine,pos-currentLineStart,lineCount++};
			lines[currentLineStart] = entry;
			lineAddresses.push_back(currentLineStart);
		}
	} else {
		while (pos < end)
		{
			char buffer[64];
			u32 value;

			u32 currentPos = pos;

			switch (type)
			{
			case DATATYPE_BYTE:
				value = memRead8(pos);
				sprintf(buffer,"0x%02X",value);
				pos++;
				break;
			case DATATYPE_HALFWORD:
				value = memRead16(pos);
				sprintf(buffer,"0x%04X",value);
				pos += 2;
				break;
			case DATATYPE_WORD:
				{
					value = memRead32(pos);
					const std::string label = symbolMap.GetLabelString(value);
					if (!label.empty())
						sprintf(buffer,"%s",label.c_str());
					else
						sprintf(buffer,"0x%08X",value);
					pos += 4;
				}
				break;
			default:
				// Avoid a call to strlen with random data
				buffer[0] = 0;
				break;
			}

			size_t len = strlen(buffer);
			if (currentLine.size() != 0 && currentLine.size()+len >= maxChars)
			{
				DataEntry entry = {currentLine,currentPos-currentLineStart,lineCount++};
				lines[currentLineStart] = entry;
				lineAddresses.push_back(currentLineStart);

				currentLine = "";
				currentLineStart = currentPos;
			}

			if (currentLine.size() != 0)
				currentLine += ",";
			currentLine += buffer;
		}

		if (currentLine.size() != 0)
		{
			DataEntry entry = {currentLine,pos-currentLineStart,lineCount++};
			lines[currentLineStart] = entry;
			lineAddresses.push_back(currentLineStart);
		}
	}
}


DisassemblyComment::DisassemblyComment(DebugInterface* _cpu, u32 _address, u32 _size, std::string _name, std::string _param)
	: cpu(_cpu), address(_address), size(_size), name(_name), param(_param)
{

}
