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

#pragma once

#include "Pcsx2Defs.h"

#include <map>			/* used by BaseBlockEx */
#include <cstring>		/* memcpy, memset, memmove */

// Every potential jump point in the PS2's addressable memory has a BASEBLOCK
// associated with it. So that means a BASEBLOCK for every 4 bytes of PS2
// addressable memory.
struct BASEBLOCK
{
	uptr m_pFnptr;
};

// extra block info (only valid for start of fn)
struct BASEBLOCKEX
{
	u32  startpc;
	uptr fnptr;
	u16  size;	 // The size in dwords (equivalent to the number of instructions)
	u16  x86size; // The size in byte of the translated x86 instructions
};

class BaseBlockArray {
	s32 _Reserved;
	s32 _Size;
	BASEBLOCKEX *blocks;

	__fi void resize(s32 size)
	{
		BASEBLOCKEX *newMem = new BASEBLOCKEX[size];
		if(blocks) {
			memcpy(newMem, blocks, _Reserved * sizeof(BASEBLOCKEX));
			delete[] blocks;
		}
		blocks = newMem;
	}

	void reserve(u32 size)
	{
		resize(size);
		_Reserved = size;
	}
public:
	~BaseBlockArray()
	{
		if(blocks) {
			delete[] blocks;
		}
	}

	BaseBlockArray (s32 size) : _Reserved(0),
		_Size(0), blocks(NULL)
	{
		reserve(size);
	}

	BASEBLOCKEX *insert(u32 startpc, uptr fnptr)
	{
		if(_Size + 1 >= _Reserved) {
			reserve(_Reserved + 0x2000); // some games requires even more!
		}

		// Insert the the new BASEBLOCKEX by startpc order
		int imin = 0, imax = _Size, imid;

		while (imin < imax) {
			imid = (imin+imax)>>1;

			if (blocks[imid].startpc > startpc)
				imax = imid;
			else
				imin = imid + 1;
		}
	
		if(imin < _Size) {
			// make a hole for a new block.
			memmove(blocks + imin + 1, blocks + imin, (_Size - imin) * sizeof(BASEBLOCKEX));
		}

		memset((blocks + imin), 0, sizeof(BASEBLOCKEX));
		blocks[imin].startpc = startpc;
		blocks[imin].fnptr = fnptr;

		_Size++;
		return &blocks[imin];
	}

	__fi BASEBLOCKEX &operator[](int idx) const
	{
		return *(blocks + idx);
	}

	void clear()
	{
		_Size = 0;
	}

	__fi u32 size() const
	{
		return _Size;
	}

	__fi void erase(s32 first, s32 last)
	{
		int range = last - first;

		if(last < _Size) {
			memmove(blocks + first, blocks + last, (_Size - last) * sizeof(BASEBLOCKEX));
		}

		_Size -= range;
	}
};

class BaseBlocks
{
protected:
	typedef std::multimap<u32, uptr>::iterator linkiter_t;

	// switch to a hash map later?
	std::multimap<u32, uptr> links;
	uptr recompiler;
	BaseBlockArray blocks;

public:
	BaseBlocks() :
		recompiler(0)
	,	blocks(0x4000)
	{
	}

	void SetJITCompile( void (*recompiler_)() )
	{
		recompiler = (uptr)recompiler_;
	}

	BASEBLOCKEX* New(u32 startpc, uptr fnptr);
	int LastIndex (u32 startpc) const;
	//BASEBLOCKEX* GetByX86(uptr ip);

	__fi int Index (u32 startpc) const
	{
		int idx = LastIndex(startpc);

		if ((idx == -1) || (startpc < blocks[idx].startpc) ||
			((blocks[idx].size) && (startpc >= blocks[idx].startpc + blocks[idx].size * 4)))
			return -1;
		else
			return idx;
	}

	__fi BASEBLOCKEX* operator[](int idx)
	{
		if (idx < 0 || idx >= (int)blocks.size())
			return 0;

		return &blocks[idx];
	}

	__fi BASEBLOCKEX* Get(u32 startpc)
	{
		return (*this)[Index(startpc)];
	}

	__fi void Remove(int first, int last)
	{
		int idx = first;
		do{
			//u32 startpc = blocks[idx].startpc;
			std::pair<linkiter_t, linkiter_t> range = links.equal_range(blocks[idx].startpc);
			for (linkiter_t i = range.first; i != range.second; ++i)
				*(u32*)i->second = recompiler - (i->second + 4);

		}
		while(idx++ < last);

		// TODO: remove links from this block?
		blocks.erase(first, last + 1);
	}

	void Link(u32 pc, s32* jumpptr);

	__fi void Reset()
	{
		blocks.clear();
		links.clear();
	}
};

#define PC_GETBLOCK_(x, reclut) ((BASEBLOCK*)(reclut[((u32)(x)) >> 16] + (x)*(sizeof(BASEBLOCK)/4)))

/**
 * Add a page to the recompiler lookup table
 *
 * Will associate `reclut[pagebase + pageidx]` with `mapbase[mappage << 14]`
 * Will associate `hwlut[pagebase + pageidx]` with `pageidx << 16`
 */
static void recLUT_SetPage(uptr reclut[0x10000], u32 hwlut[0x10000],
						   BASEBLOCK *mapbase, uint pagebase, uint pageidx, uint mappage)
{
	// this value is in 64k pages!
	uint page    = pagebase + pageidx;
	reclut[page] = (uptr)&mapbase[((s32)mappage - (s32)page) << 14];
	if (hwlut)
		hwlut[page] = 0u - (pagebase << 16);
}
