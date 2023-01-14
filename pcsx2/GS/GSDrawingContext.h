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
#ifdef __clang__
// Ignore format for this file, as it spams a lot of warnings about u64 and %llu.
#pragma clang diagnostic ignored "-Wformat"
#endif

#include "Pcsx2Types.h"

#include "GS.h"
#include "GSLocalMemory.h"

class alignas(32) GSDrawingContext
{
public:
	GIFRegXYOFFSET	XYOFFSET;
	GIFRegTEX0		TEX0;
	GIFRegTEX1		TEX1;
	GIFRegCLAMP		CLAMP;
	GIFRegMIPTBP1	MIPTBP1;
	GIFRegMIPTBP2	MIPTBP2;
	GIFRegSCISSOR	SCISSOR;
	GIFRegALPHA		ALPHA;
	GIFRegTEST		TEST;
	GIFRegFBA		FBA;
	GIFRegFRAME		FRAME;
	GIFRegZBUF		ZBUF;

	struct
	{
		GSVector4 in;
		GSVector4i ex;
		GSVector4 ofex;
		GSVector4i ofxy;
	} scissor;

	struct
	{
		GSOffset* fb;
		GSOffset* zb;
		GSOffset* tex;
		GSPixelOffset* fzb;
		GSPixelOffset4* fzb4;
	} offset;

	struct
	{
		GIFRegXYOFFSET	XYOFFSET;
		GIFRegTEX0		TEX0;
		GIFRegTEX1		TEX1;
		GIFRegCLAMP		CLAMP;
		GIFRegMIPTBP1	MIPTBP1;
		GIFRegMIPTBP2	MIPTBP2;
		GIFRegSCISSOR	SCISSOR;
		GIFRegALPHA		ALPHA;
		GIFRegTEST		TEST;
		GIFRegFBA		FBA;
		GIFRegFRAME		FRAME;
		GIFRegZBUF		ZBUF;
	} stack;

	bool m_fixed_tex0;

	GSDrawingContext()
	{
		m_fixed_tex0 = false;

		memset(&offset, 0, sizeof(offset));

		Reset();
	}

	void Reset()
	{
		memset(&XYOFFSET, 0, sizeof(XYOFFSET));
		memset(&TEX0, 0, sizeof(TEX0));
		memset(&TEX1, 0, sizeof(TEX1));
		memset(&CLAMP, 0, sizeof(CLAMP));
		memset(&MIPTBP1, 0, sizeof(MIPTBP1));
		memset(&MIPTBP2, 0, sizeof(MIPTBP2));
		memset(&SCISSOR, 0, sizeof(SCISSOR));
		memset(&ALPHA, 0, sizeof(ALPHA));
		memset(&TEST, 0, sizeof(TEST));
		memset(&FBA, 0, sizeof(FBA));
		memset(&FRAME, 0, sizeof(FRAME));
		memset(&ZBUF, 0, sizeof(ZBUF));
	}

	void UpdateScissor(void)
	{
		scissor.ex.U16[0] = (u16)((SCISSOR.SCAX0 << 4) + XYOFFSET.OFX - 0x8000);
		scissor.ex.U16[1] = (u16)((SCISSOR.SCAY0 << 4) + XYOFFSET.OFY - 0x8000);
		scissor.ex.U16[2] = (u16)((SCISSOR.SCAX1 << 4) + XYOFFSET.OFX - 0x8000);
		scissor.ex.U16[3] = (u16)((SCISSOR.SCAY1 << 4) + XYOFFSET.OFY - 0x8000);

		scissor.ofex = GSVector4(
			(int)((SCISSOR.SCAX0 << 4) + XYOFFSET.OFX),
			(int)((SCISSOR.SCAY0 << 4) + XYOFFSET.OFY),
			(int)((SCISSOR.SCAX1 << 4) + XYOFFSET.OFX),
			(int)((SCISSOR.SCAY1 << 4) + XYOFFSET.OFY));

		scissor.in = GSVector4(
			(int)SCISSOR.SCAX0,
			(int)SCISSOR.SCAY0,
			(int)SCISSOR.SCAX1 + 1,
			(int)SCISSOR.SCAY1 + 1);

		scissor.ofxy = GSVector4i(
			0x8000, 
			0x8000, 
			(int)XYOFFSET.OFX - 15, 
			(int)XYOFFSET.OFY - 15);
	}

	bool DepthRead() const
	{
		return TEST.ZTE && TEST.ZTST >= 2;
	}

	bool DepthWrite() const
	{
		if(TEST.ATE && TEST.ATST == ATST_NEVER && TEST.AFAIL != AFAIL_ZB_ONLY) // alpha test, all pixels fail, z buffer is not updated
		{
			return false;
		}

		return ZBUF.ZMSK == 0 && TEST.ZTE != 0; // ZTE == 0 is bug on the real hardware, write is blocked then
	}

	GIFRegTEX0 GetSizeFixedTEX0(const GSVector4& st, bool linear, bool mipmap = false);
	void ComputeFixedTEX0(const GSVector4& st);
	bool HasFixedTEX0() const { return m_fixed_tex0;}

	// Save & Restore before/after draw allow to correct/optimize current register for current draw
	// Note: we could avoid the restore part if all renderer code is updated to use a local copy instead
	void SaveReg()
	{
		stack.XYOFFSET = XYOFFSET;
		stack.TEX0 = TEX0;
		stack.TEX1 = TEX1;
		stack.CLAMP = CLAMP;
		stack.MIPTBP1 = MIPTBP1;
		stack.MIPTBP2 = MIPTBP2;
		stack.SCISSOR = SCISSOR;
		stack.ALPHA = ALPHA;
		stack.TEST = TEST;
		stack.FBA = FBA;
		stack.FRAME = FRAME;
		stack.ZBUF = ZBUF;

		// This function is called before the draw so take opportunity to reset m_fixed_tex0
		m_fixed_tex0 = false;
	}

	void RestoreReg()
	{
		XYOFFSET = stack.XYOFFSET;
		TEX0 = stack.TEX0;
		TEX1 = stack.TEX1;
		CLAMP = stack.CLAMP;
		MIPTBP1 = stack.MIPTBP1;
		MIPTBP2 = stack.MIPTBP2;
		SCISSOR = stack.SCISSOR;
		ALPHA = stack.ALPHA;
		TEST = stack.TEST;
		FBA = stack.FBA;
		FRAME = stack.FRAME;
		ZBUF = stack.ZBUF;
	}
};
