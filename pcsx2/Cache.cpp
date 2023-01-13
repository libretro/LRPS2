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


#include "Common.h"
#include "Utilities/MemcpyFast.h"
#include "Cache.h"
#include "vtlb.h"

using namespace vtlb_private;

namespace
{

	union alignas(64) CacheData
	{
		u8 bytes[64];

		constexpr CacheData(): bytes{0} {}
	};

	struct CacheTag
	{
		uptr rawValue = 0;

		CacheTag() = default;

		// The lower parts of a cache tags structure is as follows:
		// 31 - 12: The physical address cache tag.
		// 11 - 7: Unused.
		// 6: Dirty flag.
		// 5: Valid flag.
		// 4: LRF flag - least recently filled flag.
		// 3: Lock flag.
		// 2-0: Unused.

		enum Flags : decltype(rawValue)
		{
			DIRTY_FLAG = 0x40,
			VALID_FLAG = 0x20,
			LRF_FLAG = 0x10,
			LOCK_FLAG = 0x8,
			ALL_FLAGS = 0xFFF
		};

		bool matches(uptr other) const
		{
			uptr adr      = rawValue & ~ALL_FLAGS;
			bool is_valid = rawValue & VALID_FLAG;
			return is_valid && adr == (other & ~ALL_FLAGS);
		}
	};

	struct CacheLine
	{
		CacheTag& tag;
		CacheData& data;
		int set;

		void writeBackIfNeeded()
		{
			bool is_dirty_and_valid = (tag.rawValue & (CacheTag::DIRTY_FLAG | CacheTag::VALID_FLAG)) == (CacheTag::DIRTY_FLAG | CacheTag::VALID_FLAG);
			if (!is_dirty_and_valid)
				return;

			uptr adr    = tag.rawValue & ~CacheTag::ALL_FLAGS;
			uptr target = adr | (set << 6);
			*reinterpret_cast<CacheData*>(target) = data;
			tag.rawValue &= ~CacheTag::DIRTY_FLAG;
		}

		void load(uptr ppf)
		{
			tag.rawValue &= CacheTag::ALL_FLAGS;
			tag.rawValue |= (ppf & ~CacheTag::ALL_FLAGS);
			data = *reinterpret_cast<CacheData*>(ppf & ~0x3FULL);
			tag.rawValue |= CacheTag::VALID_FLAG;
			tag.rawValue &= ~CacheTag::DIRTY_FLAG;
		}

		void clear()
		{
			tag.rawValue &= CacheTag::LRF_FLAG;
			data = CacheData();
		}
	};

	struct CacheSet
	{
		CacheTag tags[2];
		CacheData data[2];
	};

	struct Cache
	{
		CacheSet sets[64];
	};

	static Cache cache;

}

void resetCache(void)
{
	memzero(cache);
}

static bool findInCache(const CacheSet& set, uptr ppf, int* way)
{
	auto check = [&](int checkWay) -> bool
	{
		if (!set.tags[checkWay].matches(ppf))
			return false;

		*way = checkWay;
		return true;
	};

	return check(0) || check(1);
}

static int getFreeCache(u32 mem, int* way)
{
	const int setIdx = (mem >> 6) & 0x3F;
	CacheSet& set    = cache.sets[setIdx];
	VTLBVirtual vmv  = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
	uptr ppf         = vmv.assumePtr(mem);

	if (!findInCache(set, ppf, way))
	{
		bool lrf0      = set.tags[0].rawValue & CacheTag::LRF_FLAG;
		bool lrf1      = set.tags[1].rawValue & CacheTag::LRF_FLAG;
		int newWay     = lrf0 ^ lrf1;
		*way           = newWay;
		CacheLine line = { cache.sets[setIdx].tags[newWay], cache.sets[setIdx].data[newWay], setIdx };

		line.writeBackIfNeeded();
		line.load(ppf);
		line.tag.rawValue ^= CacheTag::LRF_FLAG;
	}

	return setIdx;
}

template <typename Int>
static void writeCache(u32 mem, Int value)
{
	int way = 0;
	const int idx  = getFreeCache(mem, &way);
	CacheLine line = { cache.sets[idx].tags[way], cache.sets[idx].data[way], idx };

	line.tag.rawValue |= CacheTag::DIRTY_FLAG; // Set dirty bit for writes;
	u32 aligned        = mem & ~(sizeof(value) - 1);
	*reinterpret_cast<Int*>(&line.data.bytes[aligned & 0x3f]) = value;
}

static void writeCache8(u32 mem, u8 value)
{
	writeCache<u8>(mem, value);
}

static void writeCache16(u32 mem, u16 value)
{
	writeCache<u16>(mem, value);
}

static void writeCache32(u32 mem, u32 value)
{
	writeCache<u32>(mem, value);
}

static void writeCache64(u32 mem, const u64 value)
{
	writeCache<u64>(mem, value);
}

static void writeCache128(u32 mem, const mem128_t* value)
{
	int way        = 0;
	const int idx  = getFreeCache(mem, &way);
	CacheLine line = { cache.sets[idx].tags[way], cache.sets[idx].data[way], idx };
	line.tag.rawValue |= CacheTag::DIRTY_FLAG; // Set dirty bit for writes;
	u32 aligned    = mem & ~0xF;
	*reinterpret_cast<mem128_t*>(&line.data.bytes[aligned & 0x3f]) = *value;
}

template <typename Int>
static Int readCache(u32 mem)
{
	int way        = 0;
	const int idx  = getFreeCache(mem, &way);
	CacheLine line = { cache.sets[idx].tags[way], cache.sets[idx].data[way], idx };
	u32 aligned    = mem & ~(sizeof(Int) - 1);
	return *reinterpret_cast<Int*>(&line.data.bytes[aligned & 0x3f]);
}

static u8 readCache8(u32 mem)
{
	return readCache<u8>(mem);
}

static u16 readCache16(u32 mem)
{
	return readCache<u16>(mem);
}

static u32 readCache32(u32 mem)
{
	return readCache<u32>(mem);
}

static u64 readCache64(u32 mem)
{
	return readCache<u64>(mem);
}

template <typename Op>
static void doCacheHitOp(u32 addr, Op op)
{
	int way;
	const int index = (addr >> 6) & 0x3F;
	CacheSet  & set = cache.sets[index];
	VTLBVirtual vmv = vtlbdata.vmap[addr >> VTLB_PAGE_BITS];
	uptr        ppf = vmv.assumePtr(addr);

	if (findInCache(set, ppf, &way))
		op({ cache.sets[index].tags[way], cache.sets[index].data[way], index });
}

namespace R5900 {
namespace Interpreter
{
namespace OpcodeImpl
{

void CACHE(void)
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	switch (_Rt_) 
	{
		case 0x1a: //DHIN (Data Cache Hit Invalidate)
			doCacheHitOp(addr, [](CacheLine line)
			{
				line.clear();
			});
			break;

		case 0x18: //DHWBIN (Data Cache Hit WriteBack with Invalidate)
			doCacheHitOp(addr, [](CacheLine line)
			{
				line.writeBackIfNeeded();
				line.clear();
			});
			break;

		case 0x1c: //DHWOIN (Data Cache Hit WriteBack Without Invalidate)
			doCacheHitOp(addr, [](CacheLine line)
			{
				line.writeBackIfNeeded();
			});
			break;

		case 0x16: //DXIN (Data Cache Index Invalidate)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way   = addr & 0x1;
			CacheLine line  = { cache.sets[index].tags[way], cache.sets[index].data[way], index };
			line.clear();
			break;
		}

		case 0x11: //DXLDT (Data Cache Load Data into TagLo)
		{
			const int index     = (addr >> 6) & 0x3F;
			const int way       = addr & 0x1;
			CacheLine line      = { cache.sets[index].tags[way], cache.sets[index].data[way], index };
			cpuRegs.CP0.n.TagLo = *reinterpret_cast<u32*>(&line.data.bytes[addr & 0x3C]);
			break;
		}

		case 0x10: //DXLTG (Data Cache Load Tag into TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way   = addr & 0x1;
			CacheLine line  = { cache.sets[index].tags[way], cache.sets[index].data[way], index };

			// DXLTG demands that SYNC.L is called before this command, which forces the cache to write back, so presumably games are checking the cache has updated the memory
			// For speed, we will do it here.
			line.writeBackIfNeeded();

			// Our tags don't contain PS2 paddrs (instead they contain x86 addrs)
			cpuRegs.CP0.n.TagLo = line.tag.rawValue & CacheTag::ALL_FLAGS;
			break;
		}

		case 0x13: //DXSDT (Data Cache Store 32bits from TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way   = addr & 0x1;
			CacheLine line  = { cache.sets[index].tags[way], cache.sets[index].data[way], index };

			*reinterpret_cast<u32*>(&line.data.bytes[addr & 0x3C]) = cpuRegs.CP0.n.TagLo;

			break;
		}

		case 0x12: //DXSTG (Data Cache Store Tag from TagLo)
		{
			const int index    = (addr >> 6) & 0x3F;
			const int way      = addr & 0x1;
			CacheLine line     = { cache.sets[index].tags[way], cache.sets[index].data[way], index };

			line.tag.rawValue &= ~CacheTag::ALL_FLAGS;
			line.tag.rawValue |= (cpuRegs.CP0.n.TagLo & CacheTag::ALL_FLAGS);

			break;
		}

		case 0x14: //DXWBIN (Data Cache Index WriteBack Invalidate)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way   = addr & 0x1;
			CacheLine line  = { cache.sets[index].tags[way], cache.sets[index].data[way], index };

			line.writeBackIfNeeded();
			line.clear();
			break;
		}

		case 0x7: //IXIN (Instruction Cache Index Invalidate)
			//Not Implemented as we do not have instruction cache
		case 0xC: //BFH (BTAC Flush)
			//Not Implemented as we do not cache Branch Target Addresses.
		default:
			break;
	}
}
}		// end namespace OpcodeImpl

}}
