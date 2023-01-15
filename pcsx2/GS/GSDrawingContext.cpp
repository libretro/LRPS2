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

#include "GSDrawingContext.h"
#include "GS.h"

static int findmax(int tl, int br, int limit, int wm, int minuv, int maxuv)
{
	// return max possible texcoord
        switch (wm)
        {
		case CLAMP_REPEAT:
			if(tl < 0)
				return limit; // wrap around
                        // fall-through */
		case CLAMP_CLAMP:
			if (br > limit)
				return limit;
			break;
		case CLAMP_REGION_CLAMP:
			{
				int uv = br;
				if(uv < minuv) uv = minuv;
				if(uv > maxuv) uv = maxuv;
				return uv;
			}
		case CLAMP_REGION_REPEAT:
			if(tl < 0)
				return minuv | maxuv; // wrap around, just use (any & mask) | fix
			return std::min(br, minuv) | maxuv; // (any & mask) cannot be larger than mask, select br if that is smaller (not br & mask because there might be a larger value between tl and br when &'ed with the mask)
        }

	return br;
}

static int reduce(int uv, int size)
{
	while (size > 3 && (1 << (size - 1)) >= uv)
		size--;
	return size;
}

static int extend(int uv, int size)
{
	while (size < 10 && (1 << size) < uv)
		size++;
	return size;
}

GIFRegTEX0 GSDrawingContext::GetSizeFixedTEX0(const GSVector4& st, bool linear, bool mipmap)
{
	if(mipmap) return TEX0; // no mipmapping allowed

	// find the optimal value for TW/TH by analyzing vertex trace and clamping values, extending only for region modes where uv may be outside

	int tw = TEX0.TW;
	int th = TEX0.TH;

	int wms = (int)CLAMP.WMS;
	int wmt = (int)CLAMP.WMT;

	int minu = (int)CLAMP.MINU;
	int minv = (int)CLAMP.MINV;
	int maxu = (int)CLAMP.MAXU;
	int maxv = (int)CLAMP.MAXV;

	GSVector4 uvf = st;

	if(linear)
		uvf += GSVector4(-0.5f, 0.5f).xxyy();

	GSVector4i uv = GSVector4i(uvf.floor().xyzw(uvf.ceil()));

	uv.x = findmax(uv.x, uv.z, (1 << tw) - 1, wms, minu, maxu);
	uv.y = findmax(uv.y, uv.w, (1 << th) - 1, wmt, minv, maxv);

	if(tw + th >= 19) // smaller sizes aren't worth, they just create multiple entries in the texture cache and the saved memory is less
	{
		tw = reduce(uv.x, tw);
		th = reduce(uv.y, th);
	}

	if(wms == CLAMP_REGION_CLAMP || wms == CLAMP_REGION_REPEAT)
		tw = extend(uv.x, tw);

	if(wmt == CLAMP_REGION_CLAMP || wmt == CLAMP_REGION_REPEAT)
		th = extend(uv.y, th);

	GIFRegTEX0 res = TEX0;

	res.TW = tw;
	res.TH = th;

	return res;
}

void GSDrawingContext::ComputeFixedTEX0(const GSVector4& st)
{
	// It is quite complex to handle rescaling so this function is less stricter than GetSizeFixedTEX0,
	// therefore we remove the reduce optimization and we don't handle bilinear filtering which might create wrong interpolation at the border.
	int wms       = (int)CLAMP.WMS;
	int wmt       = (int)CLAMP.WMT;
	GSVector4i uv = GSVector4i(st.floor().xyzw(st.ceil()));

	if (wms == CLAMP_REGION_CLAMP || wms == CLAMP_REGION_REPEAT)
	{
		int tw   = TEX0.TW;
		int minu = (int)CLAMP.MINU;
		int maxu = (int)CLAMP.MAXU;
		uv.x     = findmax(uv.x, uv.z, (1 << TEX0.TW) - 1, wms, minu, maxu);
		if ((tw = extend(uv.x, tw)) != (int)TEX0.TW)
		{
			m_fixed_tex0 = true;
			TEX0.TW      = tw;
		}
	}

	if (wmt == CLAMP_REGION_CLAMP || wmt == CLAMP_REGION_REPEAT)
	{
		int th   = TEX0.TH;
		int minv = (int)CLAMP.MINV;
		int maxv = (int)CLAMP.MAXV;
		uv.y     = findmax(uv.y, uv.w, (1 << TEX0.TH) - 1, wmt, minv, maxv);
		if (((th = extend(uv.y, th)) != (int)TEX0.TH))
		{
			m_fixed_tex0 = true;
			TEX0.TH = th;
		}
	}
}
