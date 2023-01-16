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
#include <list>

#include "GSTextureCache.h"
#include "../Common/GSFunctionMap.h"
#include "../../GSState.h"

enum ACC_BLEND
{
	ACC_BLEND_NONE   = 0,
	ACC_BLEND_BASIC  = 1,
	ACC_BLEND_MEDIUM = 2,
	ACC_BLEND_HIGH   = 3,
	ACC_BLEND_FULL   = 4,
	ACC_BLEND_ULTRA  = 5
};

class GSRendererHW : public GSRenderer
{
private:
	int m_width;
	int m_height;
	bool m_reset;
	int m_upscale_multiplier;
	int m_userhacks_ts_half_bottom;

	bool m_large_framebuffer;
	bool m_userhacks_align_sprite_X;
	bool m_userhacks_enabled_gs_mem_clear;
	bool m_userHacks_merge_sprite;

	static const float SSR_UV_TOLERANCE;

	#pragma region hacks

	typedef bool (GSRendererHW::*OI_Ptr)(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	typedef void (GSRendererHW::*OO_Ptr)();
	typedef bool (GSRendererHW::*CU_Ptr)();

	// Require special argument
	bool OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* t, const GSVector4i& r_draw);
	void OI_GsMemClear(); // always on
	void OI_DoubleHalfClear(GSTexture* rt, GSTexture* ds); // always on

	bool OI_BigMuthaTruckers(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_DBZBTGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_FFXII(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_FFX(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_MetalSlug6(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_RozenMaidenGebetGarden(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_SonicUnleashed(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_StarWarsForceUnleashed(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_PointListPalette(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_SuperManReturns(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_ArTonelico2(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_JakGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);

	void OO_MajokkoALaMode2();

	bool CU_MajokkoALaMode2();
	bool CU_TalesOfAbyss();

	class Hacks
	{
		template<class T> class HackEntry
		{
		public:
			CRC::Title title;
			CRC::Region region;
			T func;

			HackEntry(CRC::Title t, CRC::Region r, T f)
			{
				title = t;
				region = r;
				func = f;
			}
		};

		template<class T> class FunctionMap : public GSFunctionMap<u32, T>
		{
			std::list<HackEntry<T> >& m_tbl;

			T GetDefaultFunction(u32 key)
			{
				CRC::Title title = (CRC::Title)(key & 0xffffff);
				CRC::Region region = (CRC::Region)(key >> 24);

				for(const auto &entry : m_tbl)
				{
					if(entry.title == title && (entry.region == CRC::RegionCount || entry.region == region))
					{
						return entry.func;
					}
				}

				return NULL;
			}

		public:
			FunctionMap(std::list<HackEntry<T> >& tbl) : m_tbl(tbl) {}
		};

		std::list<HackEntry<OI_Ptr> > m_oi_list;
		std::list<HackEntry<OO_Ptr> > m_oo_list;
		std::list<HackEntry<CU_Ptr> > m_cu_list;

		FunctionMap<OI_Ptr> m_oi_map;
		FunctionMap<OO_Ptr> m_oo_map;
		FunctionMap<CU_Ptr> m_cu_map;

	public:
		OI_Ptr m_oi;
		OO_Ptr m_oo;
		CU_Ptr m_cu;

		Hacks();

		void SetGameCRC(const CRC::Game& game);

	} m_hacks;

	#pragma endregion

	u16 Interpolate_UV(float alpha, int t0, int t1);
	float alpha0(int L, int X0, int X1);
	float alpha1(int L, int X0, int X1);
	void SwSpriteRender();
	bool CanUseSwSpriteRender(bool allow_64x64_sprite);

	template <bool linear> void RoundSpriteOffset();

protected:
	GSTextureCache* m_tc;
	GSVector4i m_r;
	GSTextureCache::Source* m_src;

	virtual void DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex) = 0;

	int m_userhacks_round_sprite_offset;
	int m_userHacks_HPO;
	bool m_userHacks_enabled_unscale_ptln;
	bool m_userHacks_preload_frame_data;

	bool m_userhacks_tcoffset;
	float m_userhacks_tcoffset_x;
	float m_userhacks_tcoffset_y;

	int m_accurate_date;
	int m_sw_blending;

	bool m_channel_shuffle;

	GSVector2i m_lod; // Min & Max level of detail
	void CustomResolutionScaling();

public:
	GSRendererHW(GSTextureCache* tc);
	virtual ~GSRendererHW();

	void SetGameCRC(u32 crc, int options);
	void UpdateRendererOptions();
	bool CanUpscale();
	int GetUpscaleMultiplier();
	void SetScaling();
	void Lines2Sprites();
	void ConvertSpriteTextureShuffle(bool& write_ba, bool& read_ba);
	GSVector4 RealignTargetTextureCoordinate(const GSTextureCache::Source* tex);
	GSVector4i ComputeBoundingBox(const GSVector2& rtscale, const GSVector2i& rtsize);
	void MergeSprite(GSTextureCache::Source* tex);
	GSVector2 GetTextureScaleFactor();

	void Reset();
	void VSync(int field);
	void ResetDevice();
	GSTexture* GetOutput(int i, int& y_offset);
	GSTexture* GetFeedbackOutput();
	void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r);
	void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false);
	void Draw();

	// Called by the texture cache to know if current texture is useful
	virtual bool IsDummyTexture() const { return false;}

};
