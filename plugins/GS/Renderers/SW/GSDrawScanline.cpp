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

#include "GSDrawScanline.h"
#include "GSTextureCacheSW.h"

// Lack of a better home
std::unique_ptr<GSScanlineConstantData> g_const(new GSScanlineConstantData());

GSDrawScanline::GSDrawScanline()
	: m_sp_map("GSSetupPrim", &m_local)
	, m_ds_map("GSDrawScanline", &m_local)
{
	memset(&m_local, 0, sizeof(m_local));

	m_local.gd = &m_global;
}

void GSDrawScanline::BeginDraw(const GSRasterizerData* data)
{
	memcpy(&m_global, &((const SharedData*)data)->global, sizeof(m_global));

	if(m_global.sel.mmin && m_global.sel.lcm)
	{
#if defined(__GNUC__) && _M_SSE >= 0x501
		// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80286
		//
		// GCC 4.9/5/6 doesn't generate correct AVX2 code for extract32<0>. It is fixed in GCC7
		// Intrinsic code is _mm_cvtsi128_si32(_mm256_castsi256_si128(m))
		// It seems recent Clang got _mm256_cvtsi256_si32(m) instead. I don't know about GCC.
		//
		// Generated code keep the integer in an XMM register but bit [64:32] aren't cleared.
		// So the srl16 shift will be huge and v will be 0.
		//
		int lod_x = m_global.lod.i.x0;
		GSVector4i v = m_global.t.minmax.srl16(lod_x);
#else
		GSVector4i v = m_global.t.minmax.srl16(m_global.lod.i.extract32<0>());//.x);
#endif

		v = v.upl16(v);

		m_local.temp.uv_minmax[0] = v.upl32(v);
		m_local.temp.uv_minmax[1] = v.uph32(v);
	}

	m_ds = m_ds_map[m_global.sel];

	if(m_global.sel.aa1)
	{
		GSScanlineSelector sel;

		sel.key = m_global.sel.key;
		sel.zwrite = 0;
		sel.edge = 1;

		m_de = m_ds_map[sel];
	}
	else
	{
		m_de = NULL;
	}

	if(m_global.sel.IsSolidRect())
	{
		m_dr = (DrawRectPtr)&GSDrawScanline::DrawRect;
	}
	else
	{
		m_dr = NULL;
	}

	// doesn't need all bits => less functions generated

	GSScanlineSelector sel;

	sel.key = 0;

	sel.iip = m_global.sel.iip;
	sel.tfx = m_global.sel.tfx;
	sel.tcc = m_global.sel.tcc;
	sel.fst = m_global.sel.fst;
	sel.fge = m_global.sel.fge;
	sel.prim = m_global.sel.prim;
	sel.fb = m_global.sel.fb;
	sel.zb = m_global.sel.zb;
	sel.zoverflow = m_global.sel.zoverflow;
	sel.notest = m_global.sel.notest;

	m_sp = m_sp_map[sel];
}

void GSDrawScanline::EndDraw(u64 frame, int actual, int total)
{
}

void GSDrawScanline::DrawRect(const GSVector4i& r, const GSVertexSW& v)
{
	// FIXME: sometimes the frame and z buffer may overlap, the outcome is undefined

	u32 m;

	#if _M_SSE >= 0x501
	m = m_global.zm;
	#else
	m = m_global.zm.U32[0];
	#endif

	if(m != 0xffffffff)
	{
		const int* zbr = m_global.zbr;
		const int* zbc = m_global.zbc;

		u32 z = v.t.U32[3]; // (u32)v.p.z;

		if(m_global.sel.zpsm != 2)
		{
			if(m == 0)
			{
				DrawRectT<u32, false>(zbr, zbc, r, z, m);
			}
			else
			{
				DrawRectT<u32, true>(zbr, zbc, r, z, m);
			}
		}
		else
		{
			if((m & 0xffff) == 0)
			{
				DrawRectT<u16, false>(zbr, zbc, r, z, m);
			}
			else
			{
				DrawRectT<u16, true>(zbr, zbc, r, z, m);
			}
		}
	}

	#if _M_SSE >= 0x501
	m = m_global.fm;
	#else
	m = m_global.fm.U32[0];
	#endif

	if(m != 0xffffffff)
	{
		const int* fbr = m_global.fbr;
		const int* fbc = m_global.fbc;

		u32 c = (GSVector4i(v.c) >> 7).rgba32();

		if(m_global.sel.fba)
		{
			c |= 0x80000000;
		}

		if(m_global.sel.fpsm != 2)
		{
			if(m == 0)
			{
				DrawRectT<u32, false>(fbr, fbc, r, c, m);
			}
			else
			{
				DrawRectT<u32, true>(fbr, fbc, r, c, m);
			}
		}
		else
		{
			c = ((c & 0xf8) >> 3) | ((c & 0xf800) >> 6) | ((c & 0xf80000) >> 9) | ((c & 0x80000000) >> 16);

			if((m & 0xffff) == 0)
			{
				DrawRectT<u16, false>(fbr, fbc, r, c, m);
			}
			else
			{
				DrawRectT<u16, true>(fbr, fbc, r, c, m);
			}
		}
	}
}

template<class T, bool masked>
void GSDrawScanline::DrawRectT(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, u32 c, u32 m)
{
	if(m == 0xffffffff) return;

	#if _M_SSE >= 0x501

	GSVector8i color((int)c);
	GSVector8i mask((int)m);

	#else

	GSVector4i color((int)c);
	GSVector4i mask((int)m);

	#endif

	if(sizeof(T) == sizeof(u16))
	{
		color = color.xxzzlh();
		mask = mask.xxzzlh();
		c = (c & 0xffff) | (c << 16);
		m = (m & 0xffff) | (m << 16);
	}

	color = color.andnot(mask);
	c = c & (~m);

	GSVector4i br = r.ralign<Align_Inside>(GSVector2i(8 * 4 / sizeof(T), 8));

	if(!br.rempty())
	{
		FillRect<T, masked>(row, col, GSVector4i(r.x, r.y, r.z, br.y), c, m);
		FillRect<T, masked>(row, col, GSVector4i(r.x, br.w, r.z, r.w), c, m);

		if(r.x < br.x || br.z < r.z)
		{
			FillRect<T, masked>(row, col, GSVector4i(r.x, br.y, br.x, br.w), c, m);
			FillRect<T, masked>(row, col, GSVector4i(br.z, br.y, r.z, br.w), c, m);
		}

		FillBlock<T, masked>(row, col, br, color, mask);
	}
	else
	{
		FillRect<T, masked>(row, col, r, c, m);
	}
}

template<class T, bool masked>
void GSDrawScanline::FillRect(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, u32 c, u32 m)
{
	if(r.x >= r.z) return;

	T* vm = (T*)m_global.vm;

	for(int y = r.y; y < r.w; y++)
	{
		T* RESTRICT d = &vm[row[y]];

		for(int x = r.x; x < r.z; x++)
		{
			d[col[x]] = (T)(!masked ? c : (c | (d[col[x]] & m)));
		}
	}
}

#if _M_SSE >= 0x501

template<class T, bool masked>
void GSDrawScanline::FillBlock(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, const GSVector8i& c, const GSVector8i& m)
{
	if(r.x >= r.z) return;

	T* vm = (T*)m_global.vm;

	for(int y = r.y; y < r.w; y += 8)
	{
		T* RESTRICT d = &vm[row[y]];

		for(int x = r.x; x < r.z; x += 8 * 4 / sizeof(T))
		{
			GSVector8i* RESTRICT p = (GSVector8i*)&d[col[x]];

			p[0] = !masked ? c : (c | (p[0] & m));
			p[1] = !masked ? c : (c | (p[1] & m));
			p[2] = !masked ? c : (c | (p[2] & m));
			p[3] = !masked ? c : (c | (p[3] & m));
			p[4] = !masked ? c : (c | (p[4] & m));
			p[5] = !masked ? c : (c | (p[5] & m));
			p[6] = !masked ? c : (c | (p[6] & m));
			p[7] = !masked ? c : (c | (p[7] & m));
		}
	}
}

#else

template<class T, bool masked>
void GSDrawScanline::FillBlock(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, const GSVector4i& c, const GSVector4i& m)
{
	if(r.x >= r.z) return;

	T* vm = (T*)m_global.vm;

	for(int y = r.y; y < r.w; y += 8)
	{
		T* RESTRICT d = &vm[row[y]];

		for(int x = r.x; x < r.z; x += 8 * 4 / sizeof(T))
		{
			GSVector4i* RESTRICT p = (GSVector4i*)&d[col[x]];

			for(int i = 0; i < 16; i += 4)
			{
				p[i + 0] = !masked ? c : (c | (p[i + 0] & m));
				p[i + 1] = !masked ? c : (c | (p[i + 1] & m));
				p[i + 2] = !masked ? c : (c | (p[i + 2] & m));
				p[i + 3] = !masked ? c : (c | (p[i + 3] & m));
			}
		}
	}
}

#endif
