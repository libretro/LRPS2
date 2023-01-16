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

#include "GSClut.h"
#include "GSLocalMemory.h"

#define CLUT_ALLOC_SIZE (2 * 4096)

static const u8 clutTableT32I8[128] =
{
	0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15,
	64, 65, 68, 69, 72, 73, 76, 77, 66, 67, 70, 71, 74, 75, 78, 79,
	16, 17, 20, 21, 24, 25, 28, 29, 18, 19, 22, 23, 26, 27, 30, 31,
	80, 81, 84, 85, 88, 89, 92, 93, 82, 83, 86, 87, 90, 91, 94, 95,
	32, 33, 36, 37, 40, 41, 44, 45, 34, 35, 38, 39, 42, 43, 46, 47,
	96, 97, 100, 101, 104, 105, 108, 109, 98, 99, 102, 103, 106, 107, 110, 111,
	48, 49, 52, 53, 56, 57, 60, 61, 50, 51, 54, 55, 58, 59, 62, 63,
	112, 113, 116, 117, 120, 121, 124, 125, 114, 115, 118, 119, 122, 123, 126, 127
};

static const u8 clutTableT32I4[16] =
{
	0, 1, 4, 5, 8, 9, 12, 13,
	2, 3, 6, 7, 10, 11, 14, 15
};

static const u8 clutTableT16I8[32] =
{
	0, 2, 8, 10, 16, 18, 24, 26,
	4, 6, 12, 14, 20, 22, 28, 30,
	1, 3, 9, 11, 17, 19, 25, 27,
	5, 7, 13, 15, 21, 23, 29, 31
};

static const u8 clutTableT16I4[16] =
{
	0, 2, 8, 10, 16, 18, 24, 26,
	4, 6, 12, 14, 20, 22, 28, 30
};

GSClut::GSClut(GSLocalMemory* mem)
	: m_mem(mem)
{
	u8* p = (u8*)vmalloc(CLUT_ALLOC_SIZE, false);

	m_clut = (u16*)&p[0];      // 1k + 1k for mirrored area simulating wrapping memory
	m_buff32 = (u32*)&p[2048]; // 1k
	m_buff64 = (u64*)&p[4096]; // 2k
	m_write.dirty = true;
	m_read.dirty = true;

	for (int i = 0; i < 16; i++)
	{
		for (int j = 0; j < 64; j++)
		{
			// The GS seems to check the lower 3 bits to tell if the format is 8/4bit
			// for the reload.
			const bool eight_bit = (j & 0x7) == 0x3;
			const bool four_bit = (j & 0x7) == 0x4;

			switch (i)
			{
				case PSM_PSMCT32:
				case PSM_PSMCT24: // undocumented (KH?)
					if (eight_bit)
						m_wc[0][i][j] = &GSClut::WriteCLUT32_I8_CSM1;
					else if (four_bit)
						m_wc[0][i][j] = &GSClut::WriteCLUT32_I4_CSM1;
					else
						m_wc[0][i][j] = &GSClut::WriteCLUT_NULL;
					break;
				case PSM_PSMCT16:
					if (eight_bit)
						m_wc[0][i][j] = &GSClut::WriteCLUT16_I8_CSM1;
					else if (four_bit)
						m_wc[0][i][j] = &GSClut::WriteCLUT16_I4_CSM1;
					else
						m_wc[0][i][j] = &GSClut::WriteCLUT_NULL;
					break;
				case PSM_PSMCT16S:
					if (eight_bit)
						m_wc[0][i][j] = &GSClut::WriteCLUT16S_I8_CSM1;
					else if (four_bit)
						m_wc[0][i][j] = &GSClut::WriteCLUT16S_I4_CSM1;
					else
						m_wc[0][i][j] = &GSClut::WriteCLUT_NULL;
					break;
				default:
					m_wc[0][i][j] = &GSClut::WriteCLUT_NULL;
			}

			// TODO: test this
			m_wc[1][i][j] = &GSClut::WriteCLUT_NULL;
		}
	}

	m_wc[1][PSM_PSMCT32][PSM_PSMT8] = &GSClut::WriteCLUT32_CSM2<256>;
	m_wc[1][PSM_PSMCT32][PSM_PSMT8H] = &GSClut::WriteCLUT32_CSM2<256>;
	m_wc[1][PSM_PSMCT32][PSM_PSMT4] = &GSClut::WriteCLUT32_CSM2<16>;
	m_wc[1][PSM_PSMCT32][PSM_PSMT4HL] = &GSClut::WriteCLUT32_CSM2<16>;
	m_wc[1][PSM_PSMCT32][PSM_PSMT4HH] = &GSClut::WriteCLUT32_CSM2<16>;
	m_wc[1][PSM_PSMCT24][PSM_PSMT8] = &GSClut::WriteCLUT32_CSM2<256>;
	m_wc[1][PSM_PSMCT24][PSM_PSMT8H] = &GSClut::WriteCLUT32_CSM2<256>;
	m_wc[1][PSM_PSMCT24][PSM_PSMT4] = &GSClut::WriteCLUT32_CSM2<16>;
	m_wc[1][PSM_PSMCT24][PSM_PSMT4HL] = &GSClut::WriteCLUT32_CSM2<16>;
	m_wc[1][PSM_PSMCT24][PSM_PSMT4HH] = &GSClut::WriteCLUT32_CSM2<16>;
	m_wc[1][PSM_PSMCT16][PSM_PSMT8] = &GSClut::WriteCLUT16_CSM2<256>;
	m_wc[1][PSM_PSMCT16][PSM_PSMT8H] = &GSClut::WriteCLUT16_CSM2<256>;
	m_wc[1][PSM_PSMCT16][PSM_PSMT4] = &GSClut::WriteCLUT16_CSM2<16>;
	m_wc[1][PSM_PSMCT16][PSM_PSMT4HL] = &GSClut::WriteCLUT16_CSM2<16>;
	m_wc[1][PSM_PSMCT16][PSM_PSMT4HH] = &GSClut::WriteCLUT16_CSM2<16>;
	m_wc[1][PSM_PSMCT16S][PSM_PSMT8] = &GSClut::WriteCLUT16S_CSM2<256>;
	m_wc[1][PSM_PSMCT16S][PSM_PSMT8H] = &GSClut::WriteCLUT16S_CSM2<256>;
	m_wc[1][PSM_PSMCT16S][PSM_PSMT4] = &GSClut::WriteCLUT16S_CSM2<16>;
	m_wc[1][PSM_PSMCT16S][PSM_PSMT4HL] = &GSClut::WriteCLUT16S_CSM2<16>;
	m_wc[1][PSM_PSMCT16S][PSM_PSMT4HH] = &GSClut::WriteCLUT16S_CSM2<16>;
}

GSClut::~GSClut()
{
	vmfree(m_clut, CLUT_ALLOC_SIZE);
}

void GSClut::Invalidate()
{
	m_write.dirty = true;
}

void GSClut::Invalidate(u32 block)
{
	if (block == m_write.TEX0.CBP)
	{
		m_write.dirty = true;
	}
}

bool GSClut::WriteTest(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	// Check if PSM is an indexed format BEFORE the load condition, updating CBP0/1 on an invalid format is not allowed
	// and can break games. Corvette (NTSC) is a good example of this.
	if ((TEX0.PSM & 0x7) < 3)
		return false;

	switch (TEX0.CLD)
	{
		case 2:
			m_CBP[0] = TEX0.CBP;
			break;
		case 3:
			m_CBP[1] = TEX0.CBP;
			break;
		case 4:
			if (m_CBP[0] == TEX0.CBP)
				return false;
			m_CBP[0] = TEX0.CBP;
			break;
		case 5:
			if (m_CBP[1] == TEX0.CBP)
				return false;
			m_CBP[1] = TEX0.CBP;
			break;
		case 0:
		case 6: // FFX-2 menu
		case 7: // ford mustang racing // Bouken Jidai Katsugeki Goemon
			return false; 
		case 1:
		default:
			break;
	}

	// CLUT only reloads if PSM is a valid index type, avoid unnecessary flushes
	return ((TEX0.PSM & 0x7) >= 3) && m_write.IsDirty(TEX0, TEXCLUT);
}

void GSClut::Write(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	m_write.TEX0 = TEX0;
	m_write.TEXCLUT = TEXCLUT;
	m_write.dirty = false;
	m_read.dirty = true;

	(this->*m_wc[TEX0.CSM][TEX0.CPSM][TEX0.PSM])(TEX0, TEXCLUT);
}

void GSClut::WriteCLUT32_I8_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	ALIGN_STACK(32);
	WriteCLUT_T32_I8_CSM1((u32*)m_mem->BlockPtr32(0, 0, TEX0.CBP, 1), m_clut, (TEX0.CSA & 15));
}

void GSClut::WriteCLUT32_I4_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	ALIGN_STACK(32);

	WriteCLUT_T32_I4_CSM1((u32*)m_mem->BlockPtr32(0, 0, TEX0.CBP, 1), m_clut + ((TEX0.CSA & 15) << 4));
}

void GSClut::WriteCLUT16_I8_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	WriteCLUT_T16_I8_CSM1((u16*)m_mem->BlockPtr16(0, 0, TEX0.CBP, 1), m_clut + (TEX0.CSA << 4));
}

void GSClut::WriteCLUT16_I4_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	WriteCLUT_T16_I4_CSM1((u16*)m_mem->BlockPtr16(0, 0, TEX0.CBP, 1), m_clut + (TEX0.CSA << 4));
}

void GSClut::WriteCLUT16S_I8_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	WriteCLUT_T16_I8_CSM1((u16*)m_mem->BlockPtr16S(0, 0, TEX0.CBP, 1), m_clut + (TEX0.CSA << 4));
}

void GSClut::WriteCLUT16S_I4_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	WriteCLUT_T16_I4_CSM1((u16*)m_mem->BlockPtr16S(0, 0, TEX0.CBP, 1), m_clut + (TEX0.CSA << 4));
}

template <int n>
void GSClut::WriteCLUT32_CSM2(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	GSOffset* off = m_mem->GetOffset(TEX0.CBP, TEXCLUT.CBW, PSM_PSMCT32);

	u32* RESTRICT s = &m_mem->m_vm32[off->pixel.row[TEXCLUT.COV]];
	int* RESTRICT col = &off->pixel.col[0][TEXCLUT.COU << 4];

	u16* RESTRICT clut = m_clut + ((TEX0.CSA & 15) << 4);

	for (int i = 0; i < n; i++)
	{
		u32 c = s[col[i]];

		clut[i] = (u16)(c & 0xffff);
		clut[i + 256] = (u16)(c >> 16);
	}
}

template <int n>
void GSClut::WriteCLUT16_CSM2(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	GSOffset* off = m_mem->GetOffset(TEX0.CBP, TEXCLUT.CBW, PSM_PSMCT16);

	u16* RESTRICT s = &m_mem->m_vm16[off->pixel.row[TEXCLUT.COV]];
	int* RESTRICT col = &off->pixel.col[0][TEXCLUT.COU << 4];

	u16* RESTRICT clut = m_clut + (TEX0.CSA << 4);

	for (int i = 0; i < n; i++)
	{
		clut[i] = s[col[i]];
	}
}

template <int n>
void GSClut::WriteCLUT16S_CSM2(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	GSOffset* off = m_mem->GetOffset(TEX0.CBP, TEXCLUT.CBW, PSM_PSMCT16S);

	u16* RESTRICT s = &m_mem->m_vm16[off->pixel.row[TEXCLUT.COV]];
	int* RESTRICT col = &off->pixel.col[0][TEXCLUT.COU << 4];

	u16* RESTRICT clut = m_clut + (TEX0.CSA << 4);

	for (int i = 0; i < n; i++)
	{
		clut[i] = s[col[i]];
	}
}

void GSClut::WriteCLUT_NULL(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	// xenosaga3, bios
 	// CLUT write ignored
}

void GSClut::Read32(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA)
{
	if (m_read.IsDirty(TEX0, TEXA))
	{
		m_read.TEX0 = TEX0;
		m_read.TEXA = TEXA;
		m_read.dirty = false;
		m_read.adirty = true;

		u16* clut = m_clut;

		if (TEX0.CPSM == PSM_PSMCT32 || TEX0.CPSM == PSM_PSMCT24)
		{
			switch (TEX0.PSM)
			{
				case PSM_PSMT8:
				case PSM_PSMT8H:
					ReadCLUT_T32_I8(clut, m_buff32, (TEX0.CSA & 15) << 4);
					break;
				case PSM_PSMT4:
				case PSM_PSMT4HL:
				case PSM_PSMT4HH:
					clut += (TEX0.CSA & 15) << 4;
					// TODO: merge these functions
					ReadCLUT_T32_I4(clut, m_buff32);
					ExpandCLUT64_T32_I8(m_buff32, (u64*)m_buff64); // sw renderer does not need m_buff64 anymore
					break;
			}
		}
		else if (TEX0.CPSM == PSM_PSMCT16 || TEX0.CPSM == PSM_PSMCT16S)
		{
			switch (TEX0.PSM)
			{
				case PSM_PSMT8:
				case PSM_PSMT8H:
					clut += TEX0.CSA << 4;
					Expand16(clut, m_buff32, 256, TEXA);
					break;
				case PSM_PSMT4:
				case PSM_PSMT4HL:
				case PSM_PSMT4HH:
					clut += TEX0.CSA << 4;
					// TODO: merge these functions
					Expand16(clut, m_buff32, 16, TEXA);
					ExpandCLUT64_T32_I8(m_buff32, (u64*)m_buff64); // sw renderer does not need m_buff64 anymore
					break;
			}
		}
	}
}

void GSClut::GetAlphaMinMax32(int& amin_out, int& amax_out)
{
	// call only after Read32
	if (m_read.adirty)
	{
		m_read.adirty = false;

		if (GSLocalMemory::m_psm[m_read.TEX0.CPSM].trbpp == 24 && m_read.TEXA.AEM == 0)
		{
			m_read.amin = m_read.TEXA.TA0;
			m_read.amax = m_read.TEXA.TA0;
		}
		else
		{
			const GSVector4i* p = (const GSVector4i*)m_buff32;

			GSVector4i amin, amax;

			if (GSLocalMemory::m_psm[m_read.TEX0.PSM].pal == 256)
			{
				amin = GSVector4i::xffffffff();
				amax = GSVector4i::zero();

				for (int i = 0; i < 16; i++)
				{
					GSVector4i v0 = (p[i * 4 + 0] >> 24).ps32(p[i * 4 + 1] >> 24);
					GSVector4i v1 = (p[i * 4 + 2] >> 24).ps32(p[i * 4 + 3] >> 24);
					GSVector4i v2 = v0.pu16(v1);

					amin = amin.min_u8(v2);
					amax = amax.max_u8(v2);
				}
			}
			else
			{
				GSVector4i v0 = (p[0] >> 24).ps32(p[1] >> 24);
				GSVector4i v1 = (p[2] >> 24).ps32(p[3] >> 24);
				GSVector4i v2 = v0.pu16(v1);

				amin = v2;
				amax = v2;
			}

			amin = amin.min_u8(amin.zwxy());
			amax = amax.max_u8(amax.zwxy());
			amin = amin.min_u8(amin.zwxyl());
			amax = amax.max_u8(amax.zwxyl());
			amin = amin.min_u8(amin.yxwzl());
			amax = amax.max_u8(amax.yxwzl());

			GSVector4i v0 = amin.upl8(amax).u8to16();
			GSVector4i v1 = v0.yxwz();

			m_read.amin = v0.min_i16(v1).extract16<0>();
			m_read.amax = v0.max_i16(v1).extract16<1>();
		}
	}

	amin_out = m_read.amin;
	amax_out = m_read.amax;
}

void GSClut::WriteCLUT_T32_I8_CSM1(const u32* RESTRICT src, u16* RESTRICT clut, u16 offset)
{
	// This is required when CSA is offset from the base of the CLUT so we point to the right data
	for (int i = offset; i < 16; i ++)
	{
		// WriteCLUT_T32_I4_CSM1 loads 16 at a time
		const int off = i << 4;
		// Source column
		const int s   = clutTableT32I8[off & 0x70] | (off & 0x80);

		WriteCLUT_T32_I4_CSM1(&src[s], &clut[off]);
	}
}

GS_FORCEINLINE void GSClut::WriteCLUT_T32_I4_CSM1(const u32* RESTRICT src, u16* RESTRICT clut)
{
	// 1 block

#if _M_SSE >= 0x501

	GSVector8i* s = (GSVector8i*)src;
	GSVector8i* d = (GSVector8i*)clut;

	GSVector8i v0 = s[0].acbd();
	GSVector8i v1 = s[1].acbd();

	GSVector8i::sw16(v0, v1);
	GSVector8i::sw16(v0, v1);
	GSVector8i::sw16(v0, v1);

	d[0] = v0;
	d[16] = v1;

#else

	GSVector4i* s = (GSVector4i*)src;
	GSVector4i* d = (GSVector4i*)clut;

	GSVector4i v0 = s[0];
	GSVector4i v1 = s[1];
	GSVector4i v2 = s[2];
	GSVector4i v3 = s[3];

	GSVector4i::sw16(v0, v1, v2, v3);
	GSVector4i::sw32(v0, v1, v2, v3);
	GSVector4i::sw16(v0, v2, v1, v3);

	d[0] = v0;
	d[1] = v2;
	d[32] = v1;
	d[33] = v3;

#endif
}

void GSClut::WriteCLUT_T16_I8_CSM1(const u16* RESTRICT src, u16* RESTRICT clut)
{
	// 2 blocks

	GSVector4i* s = (GSVector4i*)src;
	GSVector4i* d = (GSVector4i*)clut;

	for (int i = 0; i < 32; i += 4)
	{
		GSVector4i v0 = s[i + 0];
		GSVector4i v1 = s[i + 1];
		GSVector4i v2 = s[i + 2];
		GSVector4i v3 = s[i + 3];

		GSVector4i::sw16(v0, v1, v2, v3);
		GSVector4i::sw32(v0, v1, v2, v3);
		GSVector4i::sw16(v0, v2, v1, v3);

		d[i + 0] = v0;
		d[i + 1] = v2;
		d[i + 2] = v1;
		d[i + 3] = v3;
	}
}

GS_FORCEINLINE void GSClut::WriteCLUT_T16_I4_CSM1(const u16* RESTRICT src, u16* RESTRICT clut)
{
	// 1 block (half)

	for (int i = 0; i < 16; i++)
	{
		clut[i] = src[clutTableT16I4[i]];
	}
}

void GSClut::ReadCLUT_T32_I8(const u16* RESTRICT clut, u32* RESTRICT dst, int offset)
{
	// Okay this deserves a small explanation
	// T32 I8 can address up to 256 colors however the offset can be "more than zero" when reading
	// Previously I assumed that it would wrap around the end of the buffer to the beginning
	// but it turns out this is incorrect, the address doesn't mirror, it clamps to to the last offset,
	// probably though some sort of addressing mechanism then picks the color from the lower 0xF of the requested CLUT entry.
	// if we don't do this, the dirt on GTA SA goes transparent and actually cleans the car driving through dirt.
	for (int i = 0; i < 256; i += 16)
	{
		// Min value + offet or Last CSA * 16 (240)
		ReadCLUT_T32_I4(&clut[std::min((i + offset), 240)], &dst[i]);
	}
}

GS_FORCEINLINE void GSClut::ReadCLUT_T32_I4(const u16* RESTRICT clut, u32* RESTRICT dst)
{
	GSVector4i* s = (GSVector4i*)clut;
	GSVector4i* d = (GSVector4i*)dst;

	GSVector4i v0 = s[0];
	GSVector4i v1 = s[1];
	GSVector4i v2 = s[32];
	GSVector4i v3 = s[33];

	GSVector4i::sw16(v0, v2, v1, v3);

	d[0] = v0;
	d[1] = v1;
	d[2] = v2;
	d[3] = v3;
}

void GSClut::ExpandCLUT64_T32_I8(const u32* RESTRICT src, u64* RESTRICT dst)
{
	GSVector4i* s = (GSVector4i*)src;
	GSVector4i* d = (GSVector4i*)dst;

	GSVector4i s0 = s[0];
	GSVector4i s1 = s[1];
	GSVector4i s2 = s[2];
	GSVector4i s3 = s[3];

	ExpandCLUT64_T32(s0, s0, s1, s2, s3, &d[0]);
	ExpandCLUT64_T32(s1, s0, s1, s2, s3, &d[32]);
	ExpandCLUT64_T32(s2, s0, s1, s2, s3, &d[64]);
	ExpandCLUT64_T32(s3, s0, s1, s2, s3, &d[96]);
}

GS_FORCEINLINE void GSClut::ExpandCLUT64_T32(const GSVector4i& hi, const GSVector4i& lo0, const GSVector4i& lo1, const GSVector4i& lo2, const GSVector4i& lo3, GSVector4i* dst)
{
	ExpandCLUT64_T32(hi.xxxx(), lo0, &dst[0]);
	ExpandCLUT64_T32(hi.xxxx(), lo1, &dst[2]);
	ExpandCLUT64_T32(hi.xxxx(), lo2, &dst[4]);
	ExpandCLUT64_T32(hi.xxxx(), lo3, &dst[6]);
	ExpandCLUT64_T32(hi.yyyy(), lo0, &dst[8]);
	ExpandCLUT64_T32(hi.yyyy(), lo1, &dst[10]);
	ExpandCLUT64_T32(hi.yyyy(), lo2, &dst[12]);
	ExpandCLUT64_T32(hi.yyyy(), lo3, &dst[14]);
	ExpandCLUT64_T32(hi.zzzz(), lo0, &dst[16]);
	ExpandCLUT64_T32(hi.zzzz(), lo1, &dst[18]);
	ExpandCLUT64_T32(hi.zzzz(), lo2, &dst[20]);
	ExpandCLUT64_T32(hi.zzzz(), lo3, &dst[22]);
	ExpandCLUT64_T32(hi.wwww(), lo0, &dst[24]);
	ExpandCLUT64_T32(hi.wwww(), lo1, &dst[26]);
	ExpandCLUT64_T32(hi.wwww(), lo2, &dst[28]);
	ExpandCLUT64_T32(hi.wwww(), lo3, &dst[30]);
}

GS_FORCEINLINE void GSClut::ExpandCLUT64_T32(const GSVector4i& hi, const GSVector4i& lo, GSVector4i* dst)
{
	dst[0] = lo.upl32(hi);
	dst[1] = lo.uph32(hi);
}

GS_FORCEINLINE void GSClut::ExpandCLUT64_T16(const GSVector4i& hi, const GSVector4i& lo0, const GSVector4i& lo1, const GSVector4i& lo2, const GSVector4i& lo3, GSVector4i* dst)
{
	ExpandCLUT64_T16(hi.xxxx(), lo0, &dst[0]);
	ExpandCLUT64_T16(hi.xxxx(), lo1, &dst[2]);
	ExpandCLUT64_T16(hi.xxxx(), lo2, &dst[4]);
	ExpandCLUT64_T16(hi.xxxx(), lo3, &dst[6]);
	ExpandCLUT64_T16(hi.yyyy(), lo0, &dst[8]);
	ExpandCLUT64_T16(hi.yyyy(), lo1, &dst[10]);
	ExpandCLUT64_T16(hi.yyyy(), lo2, &dst[12]);
	ExpandCLUT64_T16(hi.yyyy(), lo3, &dst[14]);
	ExpandCLUT64_T16(hi.zzzz(), lo0, &dst[16]);
	ExpandCLUT64_T16(hi.zzzz(), lo1, &dst[18]);
	ExpandCLUT64_T16(hi.zzzz(), lo2, &dst[20]);
	ExpandCLUT64_T16(hi.zzzz(), lo3, &dst[22]);
	ExpandCLUT64_T16(hi.wwww(), lo0, &dst[24]);
	ExpandCLUT64_T16(hi.wwww(), lo1, &dst[26]);
	ExpandCLUT64_T16(hi.wwww(), lo2, &dst[28]);
	ExpandCLUT64_T16(hi.wwww(), lo3, &dst[30]);
}

GS_FORCEINLINE void GSClut::ExpandCLUT64_T16(const GSVector4i& hi, const GSVector4i& lo, GSVector4i* dst)
{
	dst[0] = lo.upl16(hi);
	dst[1] = lo.uph16(hi);
}

// TODO

GSVector4i GSClut::m_bm;
GSVector4i GSClut::m_gm;
GSVector4i GSClut::m_rm;

void GSClut::InitVectors()
{
	m_bm = GSVector4i(0x00007c00);
	m_gm = GSVector4i(0x000003e0);
	m_rm = GSVector4i(0x0000001f);
}

void GSClut::Expand16(const u16* RESTRICT src, u32* RESTRICT dst, int w, const GIFRegTEXA& TEXA)
{
	const GSVector4i rm = m_rm;
	const GSVector4i gm = m_gm;
	const GSVector4i bm = m_bm;

	GSVector4i TA0(TEXA.TA0 << 24);
	GSVector4i TA1(TEXA.TA1 << 24);

	GSVector4i c, cl, ch;

	const GSVector4i* s = (const GSVector4i*)src;
	GSVector4i* d = (GSVector4i*)dst;

	if (!TEXA.AEM)
	{
		for (int i = 0, j = w >> 3; i < j; i++)
		{
			c = s[i];
			cl = c.upl16(c);
			ch = c.uph16(c);
			d[i * 2 + 0] = ((cl & rm) << 3) | ((cl & gm) << 6) | ((cl & bm) << 9) | TA0.blend8(TA1, cl.sra16(15));
			d[i * 2 + 1] = ((ch & rm) << 3) | ((ch & gm) << 6) | ((ch & bm) << 9) | TA0.blend8(TA1, ch.sra16(15));
		}
	}
	else
	{
		for (int i = 0, j = w >> 3; i < j; i++)
		{
			c = s[i];
			cl = c.upl16(c);
			ch = c.uph16(c);
			d[i * 2 + 0] = ((cl & rm) << 3) | ((cl & gm) << 6) | ((cl & bm) << 9) | TA0.blend8(TA1, cl.sra16(15)).andnot(cl == GSVector4i::zero());
			d[i * 2 + 1] = ((ch & rm) << 3) | ((ch & gm) << 6) | ((ch & bm) << 9) | TA0.blend8(TA1, ch.sra16(15)).andnot(ch == GSVector4i::zero());
		}
	}
}

//

bool GSClut::WriteState::IsDirty(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT)
{
	constexpr u64 mask = 0x1FFFFFE000000000ull; // CSA CSM CPSM CBP

	bool is_dirty = dirty;

	if (((this->TEX0.U64 ^ TEX0.U64) & mask) || (GSLocalMemory::m_psm[this->TEX0.PSM].bpp != GSLocalMemory::m_psm[TEX0.PSM].bpp))
		is_dirty |= true;
	else if (TEX0.CSM == 1 && (TEXCLUT.U32[0] ^ this->TEXCLUT.U32[0]))
		is_dirty |= true;

	if (!is_dirty)
	{
		this->TEX0.U64 = TEX0.U64;
		this->TEXCLUT.U64 = TEXCLUT.U64;
	}

	return is_dirty;
}

bool GSClut::ReadState::IsDirty(const GIFRegTEX0& TEX0)
{
	constexpr u64 mask = 0x1FFFFFE000000000ull; // CSA CSM CPSM CBP

	bool is_dirty = dirty;

	if (((this->TEX0.U64 ^ TEX0.U64) & mask) || (GSLocalMemory::m_psm[this->TEX0.PSM].bpp != GSLocalMemory::m_psm[TEX0.PSM].bpp))
		is_dirty |= true;

	if (!is_dirty)
		this->TEX0.U64 = TEX0.U64;

	return is_dirty;
}

bool GSClut::ReadState::IsDirty(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA)
{
	constexpr u64 tex0_mask = 0x1FFFFFE000000000ull; // CSA CSM CPSM CBP
	constexpr u64 texa24_mask = 0x80FFull; // AEM TA0
	constexpr u64 texa16_mask = 0xFF000080FFull; // TA1 AEM TA0

	bool is_dirty = dirty;

	if (((this->TEX0.U64 ^ TEX0.U64) & tex0_mask) || (GSLocalMemory::m_psm[this->TEX0.PSM].bpp != GSLocalMemory::m_psm[TEX0.PSM].bpp))
		is_dirty |= true;
	else // Just to optimise the checks.
	{
		// Check TA0 and AEM in 24bit mode.
		if (TEX0.CPSM == PSM_PSMCT24 && ((this->TEXA.U64 ^ TEXA.U64) & texa24_mask))
			is_dirty |= true;
		// Check all fields in 16bit mode.
		else if (TEX0.CPSM >= PSM_PSMCT16 && ((this->TEXA.U64 ^ TEXA.U64) & texa16_mask))
			is_dirty |= true;
	}

	if (!is_dirty)
	{
		this->TEX0.U64 = TEX0.U64;
		this->TEXA.U64 = TEXA.U64;
	}

	return is_dirty;
}
