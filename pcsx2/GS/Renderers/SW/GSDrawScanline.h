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

#include "Pcsx2Types.h"

#include "../../GSState.h"
#include "GSRasterizer.h"
#include "GSScanlineEnvironment.h"
#include "GSSetupPrimCodeGenerator.h"
#include "GSDrawScanlineCodeGenerator.h"

class GSDrawScanline : public IDrawScanline
{
public:
	class SharedData : public GSRasterizerData
	{
	public:
		GSScanlineGlobalData global;
	};

protected:
	GSScanlineGlobalData m_global;
	GSScanlineLocalData m_local;

	GSCodeGeneratorFunctionMap<GSSetupPrimCodeGenerator, u64, SetupPrimPtr> m_sp_map;
	GSCodeGeneratorFunctionMap<GSDrawScanlineCodeGenerator, u64, DrawScanlinePtr> m_ds_map;

	template<class T, bool masked>
	void DrawRectT(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, u32 c, u32 m);

	template<class T, bool masked>
	GS_FORCEINLINE void FillRect(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, u32 c, u32 m);

	#if _M_SSE >= 0x501

	template<class T, bool masked>
	GS_FORCEINLINE void FillBlock(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, const GSVector8i& c, const GSVector8i& m);

	#else

	template<class T, bool masked>
	GS_FORCEINLINE void FillBlock(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, const GSVector4i& c, const GSVector4i& m);

	#endif

public:
	GSDrawScanline();
	virtual ~GSDrawScanline() = default;

	// IDrawScanline

	void BeginDraw(const GSRasterizerData* data);
	void EndDraw(u64 frame, int actual, int total);

	void DrawRect(const GSVector4i& r, const GSVertexSW& v);
};
