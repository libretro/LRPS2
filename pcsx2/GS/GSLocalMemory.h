/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include <unordered_map>
#include <array>
#include <vector>

#include "Pcsx2Types.h"

#include "GSTables.h"
#include "GSVector.h"
#include "GSBlock.h"
#include "GSClut.h"

class GSOffset : public GSAlignedClass<32>
{
public:
	struct alignas(32) Block
	{
		short row[256]; // yn (n = 0 8 16 ...)
		short* col; // blockOffset*
	};
	
	struct alignas(32) Pixel
	{
		int row[4096]; // yn (n = 0 1 2 ...) NOTE: this wraps around above 2048, only transfers should address the upper half (dark cloud 2 inventing)
		int* col[8]; // rowOffset*
	};

	union {u32 hash; struct {u32 bp:14, bw:6, psm:6;};};

	Block block;
	Pixel pixel;

	std::array<u32*,256> pages_as_bit; // texture page coverage based on the texture size. Lazy allocated

	GSOffset(u32 bp, u32 bw, u32 psm);
	virtual ~GSOffset();

	enum {EOP = 0xffffffff};

	u32* GetPages(const GSVector4i& rect, u32* pages = NULL, GSVector4i* bbox = NULL);
	void* GetPagesAsBits(const GSVector4i& rect, void* pages);
	u32* GetPagesAsBits(const GIFRegTEX0& TEX0);
};

struct GSPixelOffset
{
	// 16 bit offsets (m_vm16[...])

	GSVector2i row[2048]; // f yn | z yn
	GSVector2i col[2048]; // f xn | z xn
	u32 hash;
	u32 fbp, zbp, fpsm, zpsm, bw;
};

struct GSPixelOffset4
{
	// 16 bit offsets (m_vm16[...])

	GSVector2i row[2048]; // f yn | z yn (n = 0 1 2 ...)
	GSVector2i col[512]; // f xn | z xn (n = 0 4 8 ...)
	u32 hash;
	u32 fbp, zbp, fpsm, zpsm, bw;
};

class GSLocalMemory : public GSAlignedClass<32>
{
public:
	typedef u32 (*pixelAddress)(int x, int y, u32 bp, u32 bw);
	typedef void (GSLocalMemory::*writePixel)(int x, int y, u32 c, u32 bp, u32 bw);
	typedef void (GSLocalMemory::*writeFrame)(int x, int y, u32 c, u32 bp, u32 bw);
	typedef u32 (GSLocalMemory::*readPixel)(int x, int y, u32 bp, u32 bw) const;
	typedef u32 (GSLocalMemory::*readTexel)(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const;
	typedef void (GSLocalMemory::*writePixelAddr)(u32 addr, u32 c);
	typedef void (GSLocalMemory::*writeFrameAddr)(u32 addr, u32 c);
	typedef u32 (GSLocalMemory::*readPixelAddr)(u32 addr) const;
	typedef u32 (GSLocalMemory::*readTexelAddr)(u32 addr, const GIFRegTEXA& TEXA) const;
	typedef void (GSLocalMemory::*writeImage)(int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	typedef void (GSLocalMemory::*readImage)(int& tx, int& ty, u8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG) const;
	typedef void (GSLocalMemory::*readTexture)(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	typedef void (GSLocalMemory::*readTextureBlock)(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;

	struct alignas(128) psm_t
	{
		pixelAddress pa, bn;
		readPixel rp;
		readPixelAddr rpa;
		writePixel wp;
		writePixelAddr wpa;
		readTexel rt;
		readTexelAddr rta;
		writeFrameAddr wfa;
		writeImage wi;
		readImage ri;
		readTexture rtx, rtxP;
		readTextureBlock rtxb, rtxbP;
		u16 bpp, trbpp, pal, fmt;
		GSVector2i bs, pgs;
		int* rowOffset[8];
		short* blockOffset;
		u8 msk, depth;
	};

	static psm_t m_psm[64];

	static const int m_vmsize = 1024 * 1024 * 4;

	u8* m_vm8; 
	u16* m_vm16; 
	u32* m_vm32;

	GSClut m_clut;

protected:
	bool m_use_fifo_alloc;

	static u32 pageOffset32[32][32][64];
	static u32 pageOffset32Z[32][32][64];
	static u32 pageOffset16[32][64][64];
	static u32 pageOffset16S[32][64][64];
	static u32 pageOffset16Z[32][64][64];
	static u32 pageOffset16SZ[32][64][64];
	static u32 pageOffset8[32][64][128];
	static u32 pageOffset4[32][128][128];

	static int rowOffset32[4096];
	static int rowOffset32Z[4096];
	static int rowOffset16[4096];
	static int rowOffset16S[4096];
	static int rowOffset16Z[4096];
	static int rowOffset16SZ[4096];
	static int rowOffset8[2][4096];
	static int rowOffset4[2][4096];

	static short blockOffset32[256];
	static short blockOffset32Z[256];
	static short blockOffset16[256];
	static short blockOffset16S[256];
	static short blockOffset16Z[256];
	static short blockOffset16SZ[256];
	static short blockOffset8[256];
	static short blockOffset4[256];

	GS_FORCEINLINE static u32 Expand24To32(u32 c, const GIFRegTEXA& TEXA)
	{
		return (((!TEXA.AEM | (c & 0xffffff)) ? TEXA.TA0 : 0) << 24) | (c & 0xffffff);
	}

	GS_FORCEINLINE static u32 Expand16To32(u16 c, const GIFRegTEXA& TEXA)
	{
		return (((c & 0x8000) ? TEXA.TA1 : (!TEXA.AEM | c) ? TEXA.TA0 : 0) << 24) | ((c & 0x7c00) << 9) | ((c & 0x03e0) << 6) | ((c & 0x001f) << 3);
	}

	// TODO

	friend class GSClut;

	//

	std::unordered_map<u32, GSOffset*> m_omap;
	std::unordered_map<u32, GSPixelOffset*> m_pomap;
	std::unordered_map<u32, GSPixelOffset4*> m_po4map;
	std::unordered_map<u64, std::vector<GSVector2i>*> m_p2tmap;

public:
	GSLocalMemory();
	virtual ~GSLocalMemory();

	GSOffset* GetOffset(u32 bp, u32 bw, u32 psm);
	GSPixelOffset* GetPixelOffset(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF);
	GSPixelOffset4* GetPixelOffset4(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF);
	std::vector<GSVector2i>* GetPage2TileMap(const GIFRegTEX0& TEX0);

	// address

	static u32 BlockNumber32(int x, int y, u32 bp, u32 bw)
	{
		return bp + (y & ~0x1f) * bw + ((x >> 1) & ~0x1f) + blockTable32[(y >> 3) & 3][(x >> 3) & 7];
	}

	static u32 BlockNumber16(int x, int y, u32 bp, u32 bw)
	{
		return bp + ((y >> 1) & ~0x1f) * bw + ((x >> 1) & ~0x1f) + blockTable16[(y >> 3) & 7][(x >> 4) & 3];
	}

	static u32 BlockNumber16S(int x, int y, u32 bp, u32 bw)
	{
		return bp + ((y >> 1) & ~0x1f) * bw + ((x >> 1) & ~0x1f) + blockTable16S[(y >> 3) & 7][(x >> 4) & 3];
	}

	static u32 BlockNumber8(int x, int y, u32 bp, u32 bw)
	{
		return bp + ((y >> 1) & ~0x1f) * (bw >> 1) + ((x >> 2) & ~0x1f) + blockTable8[(y >> 4) & 3][(x >> 4) & 7];
	}

	static u32 BlockNumber4(int x, int y, u32 bp, u32 bw)
	{
		return bp + ((y >> 2) & ~0x1f) * (bw >> 1) + ((x >> 2) & ~0x1f) + blockTable4[(y >> 4) & 7][(x >> 5) & 3];
	}

	static u32 BlockNumber32Z(int x, int y, u32 bp, u32 bw)
	{
		return bp + (y & ~0x1f) * bw + ((x >> 1) & ~0x1f) + blockTable32Z[(y >> 3) & 3][(x >> 3) & 7];
	}

	static u32 BlockNumber16Z(int x, int y, u32 bp, u32 bw)
	{
		return bp + ((y >> 1) & ~0x1f) * bw + ((x >> 1) & ~0x1f) + blockTable16Z[(y >> 3) & 7][(x >> 4) & 3];
	}

	static u32 BlockNumber16SZ(int x, int y, u32 bp, u32 bw)
	{
		return bp + ((y >> 1) & ~0x1f) * bw + ((x >> 1) & ~0x1f) + blockTable16SZ[(y >> 3) & 7][(x >> 4) & 3];
	}

	u8* BlockPtr(u32 bp) const
	{
		return &m_vm8[(bp % MAX_BLOCKS) << 8];
	}

	u8* BlockPtr32(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber32(x, y, bp, bw) << 8];
	}

	u8* BlockPtr16(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber16(x, y, bp, bw) << 8];
	}

	u8* BlockPtr16S(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber16S(x, y, bp, bw) << 8];
	}

	u8* BlockPtr8(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber8(x, y, bp, bw) << 8];
	}

	u8* BlockPtr4(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber4(x, y, bp, bw) << 8];
	}

	u8* BlockPtr32Z(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber32Z(x, y, bp, bw) << 8];
	}

	u8* BlockPtr16Z(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber16Z(x, y, bp, bw) << 8];
	}

	u8* BlockPtr16SZ(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber16SZ(x, y, bp, bw) << 8];
	}

	static u32 PixelAddressOrg32(int x, int y, u32 bp, u32 bw)
	{
		return (BlockNumber32(x, y, bp, bw) << 6) + columnTable32[y & 7][x & 7];
	}

	static u32 PixelAddressOrg16(int x, int y, u32 bp, u32 bw)
	{
		return (BlockNumber16(x, y, bp, bw) << 7) + columnTable16[y & 7][x & 15];
	}

	static u32 PixelAddressOrg16S(int x, int y, u32 bp, u32 bw)
	{
		return (BlockNumber16S(x, y, bp, bw) << 7) + columnTable16[y & 7][x & 15];
	}

	static u32 PixelAddressOrg8(int x, int y, u32 bp, u32 bw)
	{
		return (BlockNumber8(x, y, bp, bw) << 8) + columnTable8[y & 15][x & 15];
	}

	static u32 PixelAddressOrg4(int x, int y, u32 bp, u32 bw)
	{
		return (BlockNumber4(x, y, bp, bw) << 9) + columnTable4[y & 15][x & 31];
	}

	static u32 PixelAddressOrg32Z(int x, int y, u32 bp, u32 bw)
	{
		return (BlockNumber32Z(x, y, bp, bw) << 6) + columnTable32[y & 7][x & 7];
	}

	static u32 PixelAddressOrg16Z(int x, int y, u32 bp, u32 bw)
	{
		return (BlockNumber16Z(x, y, bp, bw) << 7) + columnTable16[y & 7][x & 15];
	}

	static u32 PixelAddressOrg16SZ(int x, int y, u32 bp, u32 bw)
	{
		return (BlockNumber16SZ(x, y, bp, bw) << 7) + columnTable16[y & 7][x & 15];
	}

	static GS_FORCEINLINE u32 PixelAddress32(int x, int y, u32 bp, u32 bw)
	{
		u32 page = ((bp >> 5) + (y >> 5) * bw + (x >> 6)) % MAX_PAGES;
		u32 word = (page << 11) + pageOffset32[bp & 0x1f][y & 0x1f][x & 0x3f];

		return word;
	}

	static GS_FORCEINLINE u32 PixelAddress16(int x, int y, u32 bp, u32 bw)
	{
		u32 page = ((bp >> 5) + (y >> 6) * bw + (x >> 6)) % MAX_PAGES;
		u32 word = (page << 12) + pageOffset16[bp & 0x1f][y & 0x3f][x & 0x3f];

		return word;
	}

	static GS_FORCEINLINE u32 PixelAddress16S(int x, int y, u32 bp, u32 bw)
	{
		u32 page = ((bp >> 5) + (y >> 6) * bw + (x >> 6)) % MAX_PAGES;
		u32 word = (page << 12) + pageOffset16S[bp & 0x1f][y & 0x3f][x & 0x3f];

		return word;
	}

	static GS_FORCEINLINE u32 PixelAddress8(int x, int y, u32 bp, u32 bw)
	{
		u32 page = ((bp >> 5) + (y >> 6) * (bw >> 1) + (x >> 7)) % MAX_PAGES;
		u32 word = (page << 13) + pageOffset8[bp & 0x1f][y & 0x3f][x & 0x7f];

		return word;
	}

	static GS_FORCEINLINE u32 PixelAddress4(int x, int y, u32 bp, u32 bw)
	{
		u32 page = ((bp >> 5) + (y >> 7) * (bw >> 1) + (x >> 7)) % MAX_PAGES;
		u32 word = (page << 14) + pageOffset4[bp & 0x1f][y & 0x7f][x & 0x7f];

		return word;
	}

	static GS_FORCEINLINE u32 PixelAddress32Z(int x, int y, u32 bp, u32 bw)
	{
		u32 page = ((bp >> 5) + (y >> 5) * bw + (x >> 6)) % MAX_PAGES;
		u32 word = (page << 11) + pageOffset32Z[bp & 0x1f][y & 0x1f][x & 0x3f];

		return word;
	}

	static GS_FORCEINLINE u32 PixelAddress16Z(int x, int y, u32 bp, u32 bw)
	{
		u32 page = ((bp >> 5) + (y >> 6) * bw + (x >> 6)) % MAX_PAGES;
		u32 word = (page << 12) + pageOffset16Z[bp & 0x1f][y & 0x3f][x & 0x3f];

		return word;
	}

	static GS_FORCEINLINE u32 PixelAddress16SZ(int x, int y, u32 bp, u32 bw)
	{
		u32 page = ((bp >> 5) + (y >> 6) * bw + (x >> 6)) % MAX_PAGES;
		u32 word = (page << 12) + pageOffset16SZ[bp & 0x1f][y & 0x3f][x & 0x3f];

		return word;
	}

	// pixel R/W

	GS_FORCEINLINE u32 ReadPixel32(u32 addr) const
	{
		return m_vm32[addr];
	}

	GS_FORCEINLINE u32 ReadPixel24(u32 addr) const
	{
		return m_vm32[addr] & 0x00ffffff;
	}

	GS_FORCEINLINE u32 ReadPixel16(u32 addr) const
	{
		return (u32)m_vm16[addr];
	}

	GS_FORCEINLINE u32 ReadPixel8(u32 addr) const
	{
		return (u32)m_vm8[addr];
	}

	GS_FORCEINLINE u32 ReadPixel4(u32 addr) const
	{
		return (m_vm8[addr >> 1] >> ((addr & 1) << 2)) & 0x0f;
	}

	GS_FORCEINLINE u32 ReadPixel8H(u32 addr) const
	{
		return m_vm32[addr] >> 24;
	}

	GS_FORCEINLINE u32 ReadPixel4HL(u32 addr) const
	{
		return (m_vm32[addr] >> 24) & 0x0f;
	}

	GS_FORCEINLINE u32 ReadPixel4HH(u32 addr) const
	{
		return (m_vm32[addr] >> 28) & 0x0f;
	}

	GS_FORCEINLINE u32 ReadFrame24(u32 addr) const
	{
		return 0x80000000 | (m_vm32[addr] & 0xffffff);
	}

	GS_FORCEINLINE u32 ReadFrame16(u32 addr) const
	{
		u32 c = (u32)m_vm16[addr];

		return ((c & 0x8000) << 16) | ((c & 0x7c00) << 9) | ((c & 0x03e0) << 6) | ((c & 0x001f) << 3);
	}

	GS_FORCEINLINE u32 ReadPixel32(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel32(PixelAddress32(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel24(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel24(PixelAddress32(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel16(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel16(PixelAddress16(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel16S(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel16(PixelAddress16S(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel8(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel8(PixelAddress8(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel4(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel4(PixelAddress4(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel8H(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel8H(PixelAddress32(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel4HL(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel4HL(PixelAddress32(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel4HH(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel4HH(PixelAddress32(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel32Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel32(PixelAddress32Z(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel24Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel24(PixelAddress32Z(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel16Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel16(PixelAddress16Z(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadPixel16SZ(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel16(PixelAddress16SZ(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadFrame24(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame24(PixelAddress32(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadFrame16(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame16(PixelAddress16(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadFrame16S(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame16(PixelAddress16S(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadFrame24Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame24(PixelAddress32Z(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadFrame16Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame16(PixelAddress16Z(x, y, bp, bw));
	}

	GS_FORCEINLINE u32 ReadFrame16SZ(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame16(PixelAddress16SZ(x, y, bp, bw));
	}

	GS_FORCEINLINE void WritePixel32(u32 addr, u32 c)
	{
		m_vm32[addr] = c;
	}

	GS_FORCEINLINE void WritePixel24(u32 addr, u32 c)
	{
		m_vm32[addr] = (m_vm32[addr] & 0xff000000) | (c & 0x00ffffff);
	}

	GS_FORCEINLINE void WritePixel16(u32 addr, u32 c)
	{
		m_vm16[addr] = (u16)c;
	}

	GS_FORCEINLINE void WritePixel8(u32 addr, u32 c)
	{
		m_vm8[addr] = (u8)c;
	}

	GS_FORCEINLINE void WritePixel4(u32 addr, u32 c)
	{
		int shift = (addr & 1) << 2; addr >>= 1;

		m_vm8[addr] = (u8)((m_vm8[addr] & (0xf0 >> shift)) | ((c & 0x0f) << shift));
	}

	GS_FORCEINLINE void WritePixel8H(u32 addr, u32 c)
	{
		m_vm32[addr] = (m_vm32[addr] & 0x00ffffff) | (c << 24);
	}

	GS_FORCEINLINE void WritePixel4HL(u32 addr, u32 c)
	{
		m_vm32[addr] = (m_vm32[addr] & 0xf0ffffff) | ((c & 0x0f) << 24);
	}

	GS_FORCEINLINE void WritePixel4HH(u32 addr, u32 c)
	{
		m_vm32[addr] = (m_vm32[addr] & 0x0fffffff) | ((c & 0x0f) << 28);
	}

	GS_FORCEINLINE void WriteFrame16(u32 addr, u32 c)
	{
		u32 rb = c & 0x00f800f8;
		u32 ga = c & 0x8000f800;

		WritePixel16(addr, (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3));
	}

	GS_FORCEINLINE void WritePixel32(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel32(PixelAddress32(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel24(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel24(PixelAddress32(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel16(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel16(PixelAddress16(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel16S(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel16(PixelAddress16S(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel8(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel8(PixelAddress8(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel4(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel4(PixelAddress4(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel8H(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel8H(PixelAddress32(x, y, bp, bw), c);
	}

    GS_FORCEINLINE void WritePixel4HL(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel4HL(PixelAddress32(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel4HH(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel4HH(PixelAddress32(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel32Z(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel32(PixelAddress32Z(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel24Z(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel24(PixelAddress32Z(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel16Z(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel16(PixelAddress16Z(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel16SZ(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel16(PixelAddress16SZ(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WriteFrame16(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WriteFrame16(PixelAddress16(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WriteFrame16S(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WriteFrame16(PixelAddress16S(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WriteFrame16Z(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WriteFrame16(PixelAddress16Z(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WriteFrame16SZ(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WriteFrame16(PixelAddress16SZ(x, y, bp, bw), c);
	}

	GS_FORCEINLINE void WritePixel32(u8* RESTRICT src, u32 pitch, GSOffset* off, const GSVector4i& r)
	{
		src -= r.left * sizeof(u32);

		for(int y = r.top; y < r.bottom; y++, src += pitch)
		{
			u32* RESTRICT s = (u32*)src;
			u32* RESTRICT d = &m_vm32[off->pixel.row[y]];
			int* RESTRICT col = off->pixel.col[0];

			for(int x = r.left; x < r.right; x++)
			{
				d[col[x]] = s[x];
			}
		}
	}

	GS_FORCEINLINE void WritePixel24(u8* RESTRICT src, u32 pitch, GSOffset* off, const GSVector4i& r)
	{
		src -= r.left * sizeof(u32);

		for(int y = r.top; y < r.bottom; y++, src += pitch)
		{
			u32* RESTRICT s = (u32*)src;
			u32* RESTRICT d = &m_vm32[off->pixel.row[y]];
			int* RESTRICT col = off->pixel.col[0];

			for(int x = r.left; x < r.right; x++)
			{
				d[col[x]] = (d[col[x]] & 0xff000000) | (s[x] & 0x00ffffff);
			}
		}
	}

	GS_FORCEINLINE void WritePixel16(u8* RESTRICT src, u32 pitch, GSOffset* off, const GSVector4i& r)
	{
		src -= r.left * sizeof(u16);

		for(int y = r.top; y < r.bottom; y++, src += pitch)
		{
			u16* RESTRICT s = (u16*)src;
			u16* RESTRICT d = &m_vm16[off->pixel.row[y]];
			int* RESTRICT col = off->pixel.col[0];

			for(int x = r.left; x < r.right; x++)
			{
				d[col[x]] = s[x];
			}
		}
	}

	GS_FORCEINLINE void WriteFrame16(u8* RESTRICT src, u32 pitch, GSOffset* off, const GSVector4i& r)
	{
		src -= r.left * sizeof(u32);

		for(int y = r.top; y < r.bottom; y++, src += pitch)
		{
			u32* RESTRICT s = (u32*)src;
			u16* RESTRICT d = &m_vm16[off->pixel.row[y]];
			int* RESTRICT col = off->pixel.col[0];

			for(int x = r.left; x < r.right; x++)
			{
				u32 rb = s[x] & 0x00f800f8;
				u32 ga = s[x] & 0x8000f800;

				d[col[x]] = (u16)((ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3));
			}
		}
	}

	GS_FORCEINLINE u32 ReadTexel32(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_vm32[addr];
	}

	GS_FORCEINLINE u32 ReadTexel24(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return Expand24To32(m_vm32[addr], TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel16(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return Expand16To32(m_vm16[addr], TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel8(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel8(addr)];
	}

	GS_FORCEINLINE u32 ReadTexel4(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4(addr)];
	}

	GS_FORCEINLINE u32 ReadTexel8H(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel8H(addr)];
	}

	GS_FORCEINLINE u32 ReadTexel4HL(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4HL(addr)];
	}

	GS_FORCEINLINE u32 ReadTexel4HH(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4HH(addr)];
	}

	GS_FORCEINLINE u32 ReadTexel32(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel32(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel24(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel24(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel16(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel16S(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16S(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel8(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel8(PixelAddress8(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel4(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4(PixelAddress4(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel8H(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel8H(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel4HL(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4HL(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel4HH(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4HH(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel32Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel32(PixelAddress32Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel24Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel24(PixelAddress32Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel16Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	GS_FORCEINLINE u32 ReadTexel16SZ(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16SZ(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	//

	template<int psm, int bsx, int bsy, int alignment>
	void WriteImageColumn(int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template<int psm, int bsx, int bsy, int alignment>
	void WriteImageBlock(int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template<int psm, int bsx, int bsy>
	void WriteImageLeftRight(int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template<int psm, int bsx, int bsy, int trbpp>
	void WriteImageTopBottom(int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template<int psm, int bsx, int bsy, int trbpp>
	void WriteImage(int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);

	void WriteImage24(int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImage8H(int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImage4HL(int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImage4HH(int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImage24Z(int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImageX(int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);

	// TODO: ReadImage32/24/...

	void ReadImageX(int& tx, int& ty, u8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG) const;

	// * => 32

	void ReadTexture32(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTextureGPU24(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture24(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture16(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8H(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HL(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HH(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTexture(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTextureBlock32(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock24(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock16(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8H(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HL(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HH(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;

	// pal ? 8 : 32

	void ReadTexture8P(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4P(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8HP(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HLP(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HHP(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTextureBlock8P(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4P(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8HP(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HLP(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HHP(u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;

	//

	template<typename T> void ReadTexture(const GSOffset* RESTRICT off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
};

