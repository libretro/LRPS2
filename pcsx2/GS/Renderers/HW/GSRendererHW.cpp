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

#include "Pcsx2Types.h"

#include <cmath>
#include <algorithm>

#include "GSRendererHW.h"
#include "options_tools.h"

const float GSRendererHW::SSR_UV_TOLERANCE = 1e-3f;


GSRendererHW::GSRendererHW(GSTextureCache* tc)
	: m_width(default_rt_size.x)
	, m_height(default_rt_size.y)
	, m_reset(false)
	, m_userhacks_ts_half_bottom(-1)
	, m_tc(tc)
	, m_src(nullptr)
	, m_userhacks_tcoffset(false)
	, m_userhacks_tcoffset_x(0)
	, m_userhacks_tcoffset_y(0)
	, m_channel_shuffle(false)
	, m_lod(GSVector2i(0,0))
{
	m_userhacks_enabled_gs_mem_clear         = true;
	m_userHacks_enabled_unscale_ptln         = true;

	UpdateRendererOptions();
}

void GSRendererHW::UpdateRendererOptions()
{
	m_large_framebuffer                             = !option_value(BOOL_PCSX2_OPT_CONSERVATIVE_BUFFER, KeyOptionBool::return_type);
	m_mipmap                                        = option_value(INT_PCSX2_OPT_MIPMAPPING, KeyOptionInt::return_type);
	m_accurate_date                                 = option_value(BOOL_PCSX2_OPT_ACCURATE_DATE, KeyOptionBool::return_type);

	m_sw_blending                                   = option_value(INT_PCSX2_OPT_BLEND_UNIT_ACCURACY, KeyOptionInt::return_type);

	m_fxaa                                          = option_value(INT_PCSX2_OPT_FXAA, KeyOptionInt::return_type);
	m_interlace                                     = option_value(INT_PCSX2_OPT_DEINTERLACING_MODE, KeyOptionInt::return_type);
	m_dithering                                     = option_value(INT_PCSX2_OPT_DITHERING, KeyOptionInt::return_type);

	m_upscale_multiplier				= option_value(INT_PCSX2_OPT_UPSCALE_MULTIPLIER, KeyOptionInt::return_type);
	option_upscale_mult                             = m_upscale_multiplier;
	
	int skipdraw_start				= option_value(INT_PCSX2_OPT_USERHACK_SKIPDRAW_START, KeyOptionInt::return_type);
	int skipdraw_layers				= option_value(INT_PCSX2_OPT_USERHACK_SKIPDRAW_LAYERS, KeyOptionInt::return_type);
	m_userhacks_skipdraw_offset			= skipdraw_start;
	m_userhacks_skipdraw				= skipdraw_start + skipdraw_layers;
	m_userHacks_HPO					= option_value(INT_PCSX2_OPT_USERHACK_HALFPIXEL_OFFSET, KeyOptionInt::return_type);
	m_userhacks_wildhack				= option_value(BOOL_PCSX2_OPT_USERHACK_WILDARMS_OFFSET, KeyOptionBool::return_type);

	m_userhacks_ts_half_bottom			= option_value(INT_PCSX2_OPT_USERHACK_HALFSCREEN_FIX, KeyOptionInt::return_type);
	m_userhacks_auto_flush				= option_value(BOOL_PCSX2_OPT_USERHACK_AUTO_FLUSH, KeyOptionBool::return_type);
	m_userhacks_preload_frame_data                  = option_value(BOOL_PCSX2_OPT_USERHACK_PRELOAD_FRAME_DATA, KeyOptionBool::return_type);

	int toffset_x                                   = option_value(INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_X_HUNDREDS, KeyOptionInt::return_type);
	toffset_x                                      += option_value(INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_X_TENS, KeyOptionInt::return_type);

	int toffset_y                                   = option_value(INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_Y_HUNDREDS, KeyOptionInt::return_type);
	toffset_y                                      += option_value(INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_Y_TENS, KeyOptionInt::return_type);

	m_userhacks_tcoffset_x                          = toffset_x / -1000.0f;
	m_userhacks_tcoffset_y                          = toffset_y / -1000.0f;
	m_userhacks_tcoffset                            = m_userhacks_tcoffset_x < 0.0f || m_userhacks_tcoffset_y < 0.0f;
	hack_fb_conversion                              = option_value(BOOL_PCSX2_OPT_USERHACK_FB_CONVERSION, KeyOptionBool::return_type);

	if (m_upscale_multiplier == 1)
	{
		// hacks are only needed for upscaling issues.
		m_userhacks_round_sprite_offset         = 0;
		m_userhacks_align_sprite_X              = false;
		m_userHacks_merge_sprite                = false;
		m_userhacks_wildhack                    = false;
	}
	else
	{
		m_userhacks_round_sprite_offset         = option_value(INT_PCSX2_OPT_USERHACK_ROUND_SPRITE, KeyOptionInt::return_type);
		m_userhacks_align_sprite_X              = option_value(BOOL_PCSX2_OPT_USERHACK_ALIGN_SPRITE, KeyOptionBool::return_type);
		m_userHacks_merge_sprite                = option_value(BOOL_PCSX2_OPT_USERHACK_MERGE_SPRITE, KeyOptionBool::return_type);
	}
}

void GSRendererHW::SetScaling()
{
	const GSVector2i crtc_size(GetDisplayRect().width(), GetDisplayRect().height());

	// Details of (potential) perf impact of a big framebuffer
	// 1/ extra memory
	// 2/ texture cache framebuffer rescaling/copy
	// 3/ upload of framebuffer (preload hack)
	// 4/ framebuffer clear (color/depth/stencil)
	// 5/ read back of the frambuffer
	//
	// With the solution
	// 1/ Nothing to do.Except the texture cache bug (channel shuffle effect)
	//    most of the market is 1GB of VRAM (and soon 2GB)
	// 2/ limit rescaling/copy to the valid data of the framebuffer
	// 3/ ??? no solution so far
	// 4a/ stencil can be limited to valid data.
	// 4b/ is it useful to clear color? depth? (in any case, it ought to be few operation)
	// 5/ limit the read to the valid data

	// Framebuffer width is always a multiple of 64 so at certain cases it can't cover some weird width values.
	// 480P , 576P use width as 720 which is not referencable by FBW * 64. so it produces 704 ( the closest value multiple by 64).
	// In such cases, let's just use the CRTC width.
	const int fb_width = std::max({ (int)m_context->FRAME.FBW * 64, crtc_size.x , 512 });
	// GS doesn't have a specific register for the FrameBuffer height. so we get the height
	// from physical units of the display rectangle in case the game uses a heigher value of height.
	//
	// Gregory: the framebuffer must have enough room to draw
	// * at least 2 frames such as FMV (see OI_BlitFMV)
	// * high resolution game such as snowblind engine game
	//
	// Autodetection isn't a good idea because it will create flickering
	// If memory consumption is an issue, there is 1 possibility
	// * 1/ Avoid to create hundreds of RT
	//
	// Avoid to alternate between 640x1280 and 1280x1024 on snow blind engine game
	// int fb_height = (fb_width < 1024) ? 1280 : 1024;
	//
	// Until performance issue is properly fixed, let's keep an option to reduce the framebuffer size.
	int fb_height = m_large_framebuffer ? 1280 :
		(fb_width < 1024) ? std::max(512, crtc_size.y) : 1024;

	const int upscaled_fb_w = fb_width * m_upscale_multiplier;
	const int upscaled_fb_h = fb_height * m_upscale_multiplier;
	const bool good_rt_size = m_width >= upscaled_fb_w && m_height >= upscaled_fb_h;

	// No need to resize for native resolution as default size will be enough for native and we manually get RT Buffer size for custom.
	// don't resize until the display rectangle and register states are stabilized.
	if ( m_upscale_multiplier == 1 || good_rt_size)
		return;

	m_tc->RemovePartial();
	m_width = upscaled_fb_w;
	m_height = upscaled_fb_h;
}

GSRendererHW::~GSRendererHW()
{
	delete m_tc;
}

void GSRendererHW::SetGameCRC(u32 crc, int options)
{
	GSRenderer::SetGameCRC(crc, options);

	m_hacks.SetGameCRC(m_game);

	// Code for Automatic Mipmapping. Relies on game CRCs.
	if (option_value(INT_PCSX2_OPT_MIPMAPPING, KeyOptionInt::return_type) == -1) /* -1 is mipmap level automatic */
	{
		switch (CRC::Lookup(crc).title)
		{
			case CRC::AceCombatZero:
			case CRC::AceCombat4:
			case CRC::AceCombat5:
			case CRC::ApeEscape2:
			case CRC::Barnyard:
			case CRC::BrianLaraInternationalCricket:
			case CRC::DarkCloud:
			case CRC::DestroyAllHumans:
			case CRC::DestroyAllHumans2:
			case CRC::FIFA03:
			case CRC::FIFA04:
			case CRC::FIFA05:
			case CRC::HarryPotterATCOS:
			case CRC::HarryPotterATGOF:
			case CRC::HarryPotterATHBP:
			case CRC::HarryPotterATPOA:
			case CRC::HarryPotterOOTP:
			case CRC::ICO:
			case CRC::Jak1:
			case CRC::Jak3:
			case CRC::LegacyOfKainDefiance:
			case CRC::NicktoonsUnite:
			case CRC::Persona3:
			case CRC::ProjectSnowblind:
			case CRC::Quake3Revolution:
			case CRC::RatchetAndClank:
			case CRC::RatchetAndClank2:
			case CRC::RatchetAndClank3:
			case CRC::RatchetAndClank4:
			case CRC::RatchetAndClank5:
			case CRC::RickyPontingInternationalCricket:
			case CRC::Shox:
			case CRC::SlamTennis:
			case CRC::SoTC:
			case CRC::SoulReaver2:
			case CRC::TheIncredibleHulkUD:
			case CRC::TombRaiderAnniversary:
			case CRC::TribesAerialAssault:
			case CRC::Whiplash:
				m_mipmap = static_cast<int>(HWMipmapLevel::Basic);
				break;
			default:
				m_mipmap = static_cast<int>(HWMipmapLevel::Off);
				break;
		}
	}
}

bool GSRendererHW::CanUpscale()
{
	if(m_hacks.m_cu && !(this->*m_hacks.m_cu)())
		return false;
	return m_upscale_multiplier != 1;
}

int GSRendererHW::GetUpscaleMultiplier()
{
	return m_upscale_multiplier;
}

void GSRendererHW::Reset()
{
	m_reset = true;

	GSRenderer::Reset();
}

void GSRendererHW::VSync(int field)
{
	//Check if the frame buffer width or display width has changed
	SetScaling();

	if(m_reset)
	{
		m_tc->RemoveAll();

		m_reset = false;
	}

	GSRenderer::VSync(field);

	m_tc->IncAge();

	m_skip        = 0;
	m_skip_offset = 0;
}

void GSRendererHW::ResetDevice()
{
	m_tc->RemoveAll();

	GSRenderer::ResetDevice();
}

GSTexture* GSRendererHW::GetOutput(int i, int& y_offset)
{
	const GSRegDISPFB& DISPFB = m_regs->DISP[i].DISPFB;

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = DISPFB.Block();
	TEX0.TBW = DISPFB.FBW;
	TEX0.PSM = DISPFB.PSM;

	GSTexture* t = NULL;

	if(GSTextureCache::Target* rt = m_tc->LookupTarget(TEX0, m_width, m_height, GetFramebufferHeight()))
	{
		t = rt->m_texture;

		const int delta = TEX0.TBP0 - rt->m_TEX0.TBP0;
		if (delta > 0 && DISPFB.FBW != 0) {
			const int pages = delta >> 5u;
			int y_pages = pages / DISPFB.FBW;
			y_offset = y_pages * GSLocalMemory::m_psm[DISPFB.PSM].pgs.y;
		}
	}

	return t;
}

GSTexture* GSRendererHW::GetFeedbackOutput()
{
	GIFRegTEX0 TEX0;

	TEX0.TBP0 = m_regs->EXTBUF.EXBP;
	TEX0.TBW = m_regs->EXTBUF.EXBW;
	TEX0.PSM = m_regs->DISP[m_regs->EXTBUF.FBIN & 1].DISPFB.PSM;

	GSTextureCache::Target* rt = m_tc->LookupTarget(TEX0, m_width, m_height, 0);

	GSTexture* t = rt->m_texture;

	return t;
}

void GSRendererHW::Lines2Sprites()
{
	// each sprite converted to quad needs twice the space

	while (m_vertex.tail * 2 > m_vertex.maxcount)
		GrowVertexBuffer();

	// assume vertices are tightly packed and sequentially indexed (it should be the case)

	if (m_vertex.next >= 2)
	{
		size_t count = m_vertex.next;

		int i = (int)count * 2 - 4;
		GSVertex* s = &m_vertex.buff[count - 2];
		GSVertex* q = &m_vertex.buff[count * 2 - 4];
		u32* RESTRICT index = &m_index.buff[count * 3 - 6];

		for (; i >= 0; i -= 4, s -= 2, q -= 4, index -= 6)
		{
			GSVertex v0 = s[0];
			GSVertex v1 = s[1];

			v0.RGBAQ = v1.RGBAQ;
			v0.XYZ.Z = v1.XYZ.Z;
			v0.FOG = v1.FOG;

			if (PRIM->TME && !PRIM->FST) {
				GSVector4 st0 = GSVector4::loadl(&v0.ST.U64);
				GSVector4 st1 = GSVector4::loadl(&v1.ST.U64);
				GSVector4 Q = GSVector4(v1.RGBAQ.Q, v1.RGBAQ.Q, v1.RGBAQ.Q, v1.RGBAQ.Q);
				GSVector4 st = st0.upld(st1) / Q;

				GSVector4::storel(&v0.ST.U64, st);
				GSVector4::storeh(&v1.ST.U64, st);

				v0.RGBAQ.Q = 1.0f;
				v1.RGBAQ.Q = 1.0f;
			}

			q[0] = v0;
			q[3] = v1;

			// swap x, s, u

			const u16 x = v0.XYZ.X;
			v0.XYZ.X = v1.XYZ.X;
			v1.XYZ.X = x;

			const float s = v0.ST.S;
			v0.ST.S = v1.ST.S;
			v1.ST.S = s;

			const u16 u = v0.U;
			v0.U = v1.U;
			v1.U = u;

			q[1] = v0;
			q[2] = v1;

			index[0] = i + 0;
			index[1] = i + 1;
			index[2] = i + 2;
			index[3] = i + 1;
			index[4] = i + 2;
			index[5] = i + 3;
		}

		m_vertex.head = m_vertex.tail = m_vertex.next = count * 2;
		m_index.tail = count * 3;
	}
}

// Fix the vertex position/tex_coordinate from 16 bits color to 32 bits color
void GSRendererHW::ConvertSpriteTextureShuffle(bool& write_ba, bool& read_ba)
{
	const size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];
	const GIFRegXYOFFSET& o = m_context->XYOFFSET;

	// vertex position is 8 to 16 pixels, therefore it is the 16-31 bits of the colors
	const int  pos = (v[0].XYZ.X - o.OFX) & 0xFF;
	write_ba = (pos > 112 && pos < 136);

	// Read texture is 8 to 16 pixels (same as above)
	const float tw = (float)(1u << m_context->TEX0.TW);
	int tex_pos = (PRIM->FST) ? v[0].U : (int)(tw * v[0].ST.S);
	tex_pos &= 0xFF;
	read_ba = (tex_pos > 112 && tex_pos < 144);

	bool half_bottom = false;
	switch (m_userhacks_ts_half_bottom) {
		case 0:
			// Force Disabled.
			// Force Disabled will help games such as Xenosaga.
			// Xenosaga handles the half bottom as an vertex offset instead of a buffer offset which does the effect twice.
			// Half bottom won't trigger a cache miss that skip the draw because it is still the normal buffer but with a vertices offset.
			half_bottom = false;
			break;
		case 1:
			// Force Enabled.
			// Force Enabled will help games such as Superman Shadows of Apokolips, The Lord of the Rings: The Two Towers,
			// Demon Stone, Midnight Club 3.
			half_bottom = true;
			break;
		case -1:
		default:
			// Default, Automatic.
			// Here's the idea
			// TS effect is 16 bits but we emulate it on a 32 bits format
			// Normally this means we need to divide size by 2.
			//
			// Some games do two TS effects on each half of the buffer.
			// This makes a mess for us in the TC because we end up with two targets
			// when we only want one, thus half screen bug.
			//
			// 32bits emulation means we can do the effect once but double the size.
			// Test cases: Crash Twinsantiy and DBZ BT3
			const int height_delta = m_src->m_valid_rect.height() - m_r.height();
			// Test Case: NFS: HP2 splits the effect h:256 and h:192 so 64
			half_bottom = abs(height_delta) <= 64;
			break;
	}

	if (PRIM->FST) {
		for(size_t i = 0; i < count; i += 2) {
			if (write_ba)
				v[i].XYZ.X   -= 128u;
			else
				v[i+1].XYZ.X += 128u;

			if (read_ba)
				v[i].U       -= 128u;
			else
				v[i+1].U     += 128u;

			if (!half_bottom){
				// Height is too big (2x).
				const int tex_offset = v[i].V & 0xF;
				const GSVector4i offset(o.OFY, tex_offset, o.OFY, tex_offset);

				GSVector4i tmp(v[i].XYZ.Y, v[i].V, v[i + 1].XYZ.Y, v[i + 1].V);
				tmp = GSVector4i(tmp - offset).srl32(1) + offset;

				v[i].XYZ.Y = (u16)tmp.x;
				v[i].V = (u16)tmp.y;
				v[i + 1].XYZ.Y = (u16)tmp.z;
				v[i + 1].V = (u16)tmp.w;
			}
		}
	} else {
		const float offset_8pix = 8.0f / tw;

		for(size_t i = 0; i < count; i += 2) {
			if (write_ba)
				v[i].XYZ.X   -= 128u;
			else
				v[i+1].XYZ.X += 128u;

			if (read_ba)
				v[i].ST.S    -= offset_8pix;
			else
				v[i+1].ST.S  += offset_8pix;

			if (!half_bottom) {
				// Height is too big (2x).
				const GSVector4i offset(o.OFY, o.OFY);

				GSVector4i tmp(v[i].XYZ.Y, v[i + 1].XYZ.Y);
				tmp = GSVector4i(tmp - offset).srl32(1) + offset;

				//fprintf(stderr, "Before %d, After %d\n", v[i+1].XYZ.Y, tmp.y);
				v[i].XYZ.Y = (u16)tmp.x;
				v[i].ST.T /= 2.0f;
				v[i + 1].XYZ.Y = (u16)tmp.y;
				v[i + 1].ST.T /= 2.0f;
			}
		}
	}

	// Update vertex trace too. Avoid issue to compute bounding box
	if (write_ba)
		m_vt.m_min.p.x -= 8.0f;
	else
		m_vt.m_max.p.x += 8.0f;

	if (!half_bottom) {
		const float delta_Y = m_vt.m_max.p.y - m_vt.m_min.p.y;
		m_vt.m_max.p.y -= delta_Y / 2.0f;
	}

	if (read_ba)
		m_vt.m_min.t.x -= 8.0f;
	else
		m_vt.m_max.t.x += 8.0f;

	if (!half_bottom) {
		const float delta_T = m_vt.m_max.t.y - m_vt.m_min.t.y;
		m_vt.m_max.t.y -= delta_T / 2.0f;
	}
}

GSVector4 GSRendererHW::RealignTargetTextureCoordinate(const GSTextureCache::Source* tex)
{
	if (m_userHacks_HPO <= 1 || GetUpscaleMultiplier() == 1) return GSVector4(0.0f);

	const GSVertex* v             = &m_vertex.buff[0];
	const GSVector2& scale  = tex->m_texture->GetScale();
	const bool  linear            = m_vt.m_filter.linear;
	const int t_position          = v[0].U;
	GSVector4 half_offset(0.0f);

	// FIXME Let's start with something wrong same mess on X and Y
	// FIXME Maybe it will be enough to check linear

	if (PRIM->FST) {

		if (m_userHacks_HPO == 3) {
			if (!linear && t_position == 8) {
				half_offset.x = 8;
				half_offset.y = 8;
			} else if (linear && t_position == 16) {
				half_offset.x = 16;
				half_offset.y = 16;
			} else if (m_vt.m_min.p.x == -0.5f) {
				half_offset.x = 8;
				half_offset.y = 8;
			}
		} else {
			if (!linear && t_position == 8) {
				half_offset.x = 8 - 8 / scale.x;
				half_offset.y = 8 - 8 / scale.y;
			} else if (linear && t_position == 16) {
				half_offset.x = 16 - 16 / scale.x;
				half_offset.y = 16 - 16 / scale.y;
			} else if (m_vt.m_min.p.x == -0.5f) {
				half_offset.x = 8;
				half_offset.y = 8;
			}
		}
	} else if (m_vt.m_eq.q) {
		const float tw = (float)(1 << m_context->TEX0.TW);
		const float th = (float)(1 << m_context->TEX0.TH);
		const float q  = v[0].RGBAQ.Q;

		// Tales of Abyss
		half_offset.x = 0.5f * q / tw;
		half_offset.y = 0.5f * q / th;
	}

	return half_offset;
}

GSVector4i GSRendererHW::ComputeBoundingBox(const GSVector2& rtscale, const GSVector2i& rtsize)
{
	const GSVector4 scale = GSVector4(rtscale.x, rtscale.y);
	const GSVector4 offset = GSVector4(-1.0f, 1.0f); // Round value
	const GSVector4 box = m_vt.m_min.p.xyxy(m_vt.m_max.p) + offset.xxyy();
	return GSVector4i(box * scale.xyxy()).rintersect(GSVector4i(0, 0, rtsize.x, rtsize.y));
}

void GSRendererHW::MergeSprite(GSTextureCache::Source* tex)
{
	// Upscaling hack to avoid various line/grid issues
	if (m_userHacks_merge_sprite && tex && tex->m_target && (m_vt.m_primclass == GS_SPRITE_CLASS)) {
		if (PRIM->FST && GSLocalMemory::m_psm[tex->m_TEX0.PSM].fmt < 2 && ((m_vt.m_eq.value & 0xCFFFF) == 0xCFFFF)) {

			// Ideally the hack ought to be enabled in a true paving mode only. I don't know how to do it accurately
			// neither in a fast way. So instead let's just take the hypothesis that all sprites must have the same
			// size.
			// Tested on Tekken 5.
			const GSVertex* v = &m_vertex.buff[0];
			bool is_paving = true;
			// SSE optimization: shuffle m[1] to have (4*32 bits) X, Y, U, V
			const int first_dpX = v[1].XYZ.X - v[0].XYZ.X;
			const int first_dpU = v[1].U - v[0].U;
			for (size_t i = 0; i < m_vertex.next; i += 2) {
				const int dpX = v[i + 1].XYZ.X - v[i].XYZ.X;
				const int dpU = v[i + 1].U - v[i].U;
				if (dpX != first_dpX || dpU != first_dpU) {
					is_paving = false;
					break;
				}
			}

			if (is_paving) {
				// Replace all sprite with a single fullscreen sprite.
				GSVertex* s = &m_vertex.buff[0];

				s[0].XYZ.X = static_cast<u16>((16.0f * m_vt.m_min.p.x) + m_context->XYOFFSET.OFX);
				s[1].XYZ.X = static_cast<u16>((16.0f * m_vt.m_max.p.x) + m_context->XYOFFSET.OFX);
				s[0].XYZ.Y = static_cast<u16>((16.0f * m_vt.m_min.p.y) + m_context->XYOFFSET.OFY);
				s[1].XYZ.Y = static_cast<u16>((16.0f * m_vt.m_max.p.y) + m_context->XYOFFSET.OFY);

				s[0].U = static_cast<u16>(16.0f * m_vt.m_min.t.x);
				s[0].V = static_cast<u16>(16.0f * m_vt.m_min.t.y);
				s[1].U = static_cast<u16>(16.0f * m_vt.m_max.t.x);
				s[1].V = static_cast<u16>(16.0f * m_vt.m_max.t.y);

				m_vertex.head = m_vertex.tail = m_vertex.next = 2;
				m_index.tail = 2;
			}
		}
	}
}

GSVector2 GSRendererHW::GetTextureScaleFactor()
{
	GSVector2 scale_factor{ 1.0f, 1.0f };
	if (CanUpscale())
	{
		const int multiplier = GetUpscaleMultiplier();
		scale_factor.x       = multiplier;
		scale_factor.y       = multiplier;
	}
	return scale_factor;
}

void GSRendererHW::InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	m_tc->InvalidateVideoMem(m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM), r);
}

void GSRendererHW::InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut)
{
	if(clut) return; // FIXME

	m_tc->InvalidateLocalMem(m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM), r);
}

u16 GSRendererHW::Interpolate_UV(float alpha, int t0, int t1)
{
	const float t = (1.0f - alpha) * t0 + alpha * t1;
	return (u16)t & ~0xF; // cheap rounding
}

float GSRendererHW::alpha0(int L, int X0, int X1)
{
	const int x = (X0 + 15) & ~0xF; // Round up
	return float(x - X0) / (float)L;
}

float GSRendererHW::alpha1(int L, int X0, int X1)
{
	const int x = (X1 - 1) & ~0xF; // Round down. Note -1 because right pixel isn't included in primitive so 0x100 must return 0.
	return float(x - X0) / (float)L;
}

void GSRendererHW::SwSpriteRender()
{
	const bool texture_mapping_enabled = PRIM->TME;

	// Setup registers for SW rendering
	GIFRegBITBLTBUF bitbltbuf = {};

	if (texture_mapping_enabled)
	{
		bitbltbuf.SBP = m_context->TEX0.TBP0;
		bitbltbuf.SBW = m_context->TEX0.TBW;
		bitbltbuf.SPSM = m_context->TEX0.PSM;
	}

	bitbltbuf.DBP  = GIFREG_FRAME_BLOCK(m_context->FRAME);
	bitbltbuf.DBW  = m_context->FRAME.FBW;
	bitbltbuf.DPSM = m_context->FRAME.PSM;

	GIFRegTRXPOS trxpos = {};

	trxpos.DSAX = 0;
	trxpos.DSAY = 0;
	trxpos.SSAX = 0;
	trxpos.SSAY = 0;

	GIFRegTRXREG trxreg = {};

	trxreg.RRW = m_r.z;
	trxreg.RRH = m_r.w;

	// SW rendering code, mainly taken from GSState::Move(), TRXPOS.DIR{X,Y} management excluded

	int sx = trxpos.SSAX;
	int sy = trxpos.SSAY;
	int dx = trxpos.DSAX;
	int dy = trxpos.DSAY;
	const int w = trxreg.RRW;
	const int h = trxreg.RRH;

	if (texture_mapping_enabled)
		InvalidateLocalMem(bitbltbuf, GSVector4i(sx, sy, sx + w, sy + h));
	InvalidateVideoMem(bitbltbuf, GSVector4i(dx, dy, dx + w, dy + h));

	GSOffset* RESTRICT spo = texture_mapping_enabled ? m_mem.GetOffset(bitbltbuf.SBP, bitbltbuf.SBW, bitbltbuf.SPSM) : nullptr;
	GSOffset* RESTRICT dpo = m_mem.GetOffset(bitbltbuf.DBP, bitbltbuf.DBW, bitbltbuf.DPSM);

	const int* RESTRICT scol = texture_mapping_enabled ? &spo->pixel.col[0][sx] : nullptr;
	int* RESTRICT dcol = &dpo->pixel.col[0][dx];

	const bool alpha_blending_enabled = PRIM->ABE;

	const GSVertex& v = m_vertex.buff[m_index.buff[m_index.tail - 1]];  // Last vertex.
	const GSVector4i vc = GSVector4i(v.RGBAQ.R, v.RGBAQ.G, v.RGBAQ.B, v.RGBAQ.A)  // 0x000000AA000000BB000000GG000000RR
							.ps32();  // 0x00AA00BB00GG00RR00AA00BB00GG00RR

	const GSVector4i a_mask = GSVector4i::xff000000().u8to16();  // 0x00FF00000000000000FF000000000000

	const bool fb_mask_enabled = m_context->FRAME.FBMSK != 0x0;
	const GSVector4i fb_mask = GSVector4i(m_context->FRAME.FBMSK).u8to16(); // 0x00AA00BB00GG00RR00AA00BB00GG00RR

	const u8 tex0_tfx = m_context->TEX0.TFX;
	const u8 tex0_tcc = m_context->TEX0.TCC;
	const u8 alpha_b = m_context->ALPHA.B;
	const u8 alpha_c = m_context->ALPHA.C;
	const u8 alpha_fix = m_context->ALPHA.FIX;

	for (int y = 0; y < h; y++, ++sy, ++dy)
	{
		const u32* RESTRICT s = texture_mapping_enabled ? &m_mem.m_vm32[spo->pixel.row[sy]] : nullptr;
		u32* RESTRICT d = &m_mem.m_vm32[dpo->pixel.row[dy]];

		for (int x = 0; x < w; x += 2)
		{
			GSVector4i sc;
			if (texture_mapping_enabled)
			{
				// Read 2 source pixel colors
				sc = GSVector4i::loadl(&s[scol[x]]).u8to16();  // 0x00AA00BB00GG00RR00aa00bb00gg00rr

				// Apply TFX
				if (tex0_tfx == 0)
					sc = sc.mul16l(vc).srl16(7).clamp8();  // clamp((sc * vc) >> 7, 0, 255), srl16 is ok because 16 bit values are unsigned

				if (tex0_tcc == 0)
					sc = sc.blend(vc, a_mask);
			}
			else
				sc = vc;

			// No FOG

			GSVector4i dc0;
			GSVector4i dc;

			if (alpha_blending_enabled || fb_mask_enabled)
			{
				// Read 2 destination pixel colors
				dc0 = GSVector4i::loadl(&d[dcol[x]]).u8to16();  // 0x00AA00BB00GG00RR00aa00bb00gg00rr
			}

			if (alpha_blending_enabled)
			{
				// Flag C
				GSVector4i sc_alpha_vec;

				if (alpha_c == 2)
					sc_alpha_vec = GSVector4i(alpha_fix).xxxx().ps32();
				else
					sc_alpha_vec = (alpha_c == 0 ? sc : dc0)
						.yyww()     // 0x00AA00BB00AA00BB00aa00bb00aa00bb
						.srl32(16)  // 0x000000AA000000AA000000aa000000aa
						.ps32()     // 0x00AA00AA00aa00aa00AA00AA00aa00aa
						.xxyy();    // 0x00AA00AA00AA00AA00aa00aa00aa00aa

				switch (alpha_b)
				{
				case 1:
					dc = sc.sub16(dc0).mul16l(sc_alpha_vec).sra16(7).add16(dc0);  // (((Cs - Cd) * C) >> 7) + Cd, must use sra16 due to signed 16 bit values
					break;
				default:
					dc = sc.mul16l(sc_alpha_vec).sra16(7).add16(dc0);  // (((Cs - 0) * C) >> 7) + Cd, must use sra16 due to signed 16 bit values
					break;
				}
				// dc alpha channels (dc.u16[3], dc.u16[7]) dirty
			}
			else
				dc = sc;

			// No dithering

			// Clamping
			if (m_env.COLCLAMP.CLAMP)
				dc = dc.clamp8();  // clamp(dc, 0, 255)
			else
				dc = dc.sll16(8).srl16(8);  // Mask, lower 8 bits enabled per channel

			// No Alpha Correction
			dc = dc.blend(sc, a_mask);
			// dc alpha channels valid

			// Frame buffer mask
			if (fb_mask_enabled)
				dc = dc.blend(dc0, fb_mask);

			// Store 2 pixel colors
			dc = dc.pu16(GSVector4i::zero());  // 0x0000000000000000AABBGGRRaabbggrr
			GSVector4i::storel(&d[dcol[x]], dc);
		}
	}
}

bool GSRendererHW::CanUseSwSpriteRender(bool allow_64x64_sprite)
{
	const bool r_0_0_64_64 = allow_64x64_sprite ? (m_r == GSVector4i(0, 0, 64, 64)).alltrue() : false;
	if (r_0_0_64_64 && !allow_64x64_sprite)  // Rendering region 64x64 support is enabled via parameter
		return false;
	const bool r_0_0_16_16 = (m_r == GSVector4i(0, 0, 16, 16)).alltrue();
	if (!r_0_0_16_16 && !r_0_0_64_64)  // Rendering region is 16x16 or 64x64, without offset
		return false;
	if (PRIM->PRIM != GS_SPRITE
		&& ((PRIM->IIP && m_vt.m_eq.rgba != 0xffff)
			|| (PRIM->TME && !PRIM->FST && m_vt.m_eq.q != 0x1)
			|| m_vt.m_eq.z != 0x1))  // No rasterization
		return false;
	if (m_vt.m_primclass != GS_TRIANGLE_CLASS && m_vt.m_primclass != GS_SPRITE_CLASS)  // Triangle or sprite class prims
		return false;
	if (PRIM->PRIM != GS_TRIANGLESTRIP && PRIM->PRIM != GS_SPRITE)  // Triangle strip or sprite draw
		return false;
	if (m_vt.m_primclass == GS_TRIANGLE_CLASS && (PRIM->PRIM != GS_TRIANGLESTRIP || m_vertex.tail != 4))  // If triangle class, strip draw with 4 vertices (two prims, emulating single sprite prim)
		return false;
	// TODO If GS_TRIANGLESTRIP draw, check that the draw is axis aligned
	if (m_vt.m_primclass == GS_SPRITE_CLASS && (PRIM->PRIM != GS_SPRITE || m_vertex.tail != 2))  // If sprite class, sprite draw with 2 vertices (one prim)
		return false;
	if (m_context->DepthRead() || m_context->DepthWrite())  // No depth handling
		return false;
	if (m_context->FRAME.PSM != PSM_PSMCT32)  // Frame buffer format is 32 bit color
		return false;
	if (PRIM->TME)
	{ 
		// Texture mapping enabled

		if (m_context->TEX0.PSM != PSM_PSMCT32)  // Input texture format is 32 bit color
			return false;
		if (IsMipMapDraw())  // No mipmapping
			return false;
		if (abs(m_vt.m_min.t.x) > SSR_UV_TOLERANCE || abs(m_vt.m_min.t.y) > SSR_UV_TOLERANCE)  // No horizontal nor vertical offset
			return false;
		if (abs(m_vt.m_max.t.x - m_r.z) > SSR_UV_TOLERANCE || abs(m_vt.m_max.t.y - m_r.w) > SSR_UV_TOLERANCE)  // No texture width or height mag/min
			return false;
		const int tw = 1 << m_context->TEX0.TW;
		const int th = 1 << m_context->TEX0.TH;
		if (m_vt.m_max.t.x > tw || m_vt.m_max.t.y > th)  // No UV wrapping
			return false;
	}
	
	// The draw call is a good candidate for using the SwSpriteRender to replace the GPU draw
	// However, some draw attributes might not be supported yet by the SwSpriteRender,
	// so if any bug occurs in using it, enabling debug build would probably
	// make failing some of the assertions used in the SwSpriteRender to highlight its limitations.
	// In that case, either the condition can be added here to discard the draw, or the
	// SwSpriteRender can be improved by adding the missing features.
	return true;
}

template <bool linear>
void GSRendererHW::RoundSpriteOffset()
{
	const size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];

	for(size_t i = 0; i < count; i += 2) {
		// Performance note: if it had any impact on perf, someone would port it to SSE (AKA GSVector)

		// Compute the coordinate of first and last texels (in native with a linear filtering)
		const int	   ox  = m_context->XYOFFSET.OFX;
		const int    X0  = v[i].XYZ.X   - ox;
		const int	   X1  = v[i+1].XYZ.X - ox;
		const int	   Lx  = (v[i+1].XYZ.X - v[i].XYZ.X);
		const float  ax0 = alpha0(Lx, X0, X1);
		const float  ax1 = alpha1(Lx, X0, X1);
		const u16 tx0 = Interpolate_UV(ax0, v[i].U, v[i+1].U);
		const u16 tx1 = Interpolate_UV(ax1, v[i].U, v[i+1].U);

		const int	   oy  = m_context->XYOFFSET.OFY;
		const int	   Y0  = v[i].XYZ.Y   - oy;
		const int	   Y1  = v[i+1].XYZ.Y - oy;
		const int	   Ly  = (v[i+1].XYZ.Y - v[i].XYZ.Y);
		const float  ay0 = alpha0(Ly, Y0, Y1);
		const float  ay1 = alpha1(Ly, Y0, Y1);
		const u16 ty0 = Interpolate_UV(ay0, v[i].V, v[i+1].V);
		const u16 ty1 = Interpolate_UV(ay1, v[i].V, v[i+1].V);
		// Use rounded value of the newly computed texture coordinate. It ensures
		// that sampling will remains inside texture boundary
		//
		// Note for bilinear: by definition it will never work correctly! A sligh modification
		// of interpolation migth trigger a discard (with alpha testing)
		// Let's use something simple that correct really bad case (for a couple of 2D games).
		// I hope it won't create too much glitches.
		if (linear) {
			const int Lu = v[i+1].U - v[i].U;
			// Note 32 is based on taisho-mononoke
			if ((Lu > 0) && (Lu <= (Lx+32))) {
				v[i+1].U -= 8;
			}
		} else {
			if (tx0 <= tx1) {
				v[i].U   = tx0;
				v[i+1].U = tx1 + 16;
			} else {
				v[i].U   = tx0 + 15;
				v[i+1].U = tx1;
			}
		}
		if (linear) {
			const int Lv = v[i+1].V - v[i].V;
			if ((Lv > 0) && (Lv <= (Ly+32))) {
				v[i+1].V -= 8;
			}
		} else {
			if (ty0 <= ty1) {
				v[i].V   = ty0;
				v[i+1].V = ty1 + 16;
			} else {
				v[i].V   = ty0 + 15;
				v[i+1].V = ty1;
			}
		}
	}
}

void GSRendererHW::Draw()
{
	if(IsBadFrame())
		return;

	const GSDrawingEnvironment& env = m_env;
	GSDrawingContext* context = m_context;
	const GSLocalMemory::psm_t& tex_psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];

	// Skipping draw with FRAME.FBW = 0? 
	if (!context->FRAME.FBW)
		return;

	// Fix TEX0 size
	bool is_mipmap_active = m_mipmap && IsMipMapDraw();
	if(PRIM->TME && !is_mipmap_active)
		m_context->ComputeFixedTEX0(m_vt.m_min.t.xyxy(m_vt.m_max.t));

	// skip alpha test if possible
	// Note: do it first so we know if frame/depth writes are masked

	const GIFRegTEST TEST = context->TEST;
	const GIFRegFRAME FRAME = context->FRAME;
	const GIFRegZBUF ZBUF = context->ZBUF;

	u32 fm = context->FRAME.FBMSK;
	u32 zm = context->ZBUF.ZMSK || context->TEST.ZTE == 0 ? 0xffffffff : 0;

	// Note required to compute TryAlphaTest below. So do it now.
	if (PRIM->TME && tex_psm.pal > 0)
		m_mem.m_clut.Read32(context->TEX0, env.TEXA);

	//  Test if we can optimize Alpha Test as a NOP
	context->TEST.ATE = context->TEST.ATE && !GSRenderer::TryAlphaTest(fm, zm);

	context->FRAME.FBMSK = fm;
	context->ZBUF.ZMSK = zm != 0;

	// It is allowed to use the depth and rt at the same location. However at least 1 must
	// be disabled. Or the written value must be the same on both channels.
	// 1/ GoW uses a Cd blending on a 24 bits buffer (no alpha)
	// 2/ SuperMan really draws (0,0,0,0) color and a (0) 32-bits depth
	// 3/ 50cents really draws (0,0,0,128) color and a (0) 24 bits depth
	// Note: FF DoC has both buffer at same location but disable the depth test (write?) with ZTE = 0
	const bool no_rt = (context->ALPHA.IsCd() && PRIM->ABE && (context->FRAME.PSM == 1));
	const bool no_ds = !no_rt && (
			// Depth is always pass/fail (no read) and write are discarded (tekken 5).  (Note: DATE is currently implemented with a stencil buffer => a depth/stencil buffer)
			(zm != 0 && m_context->TEST.ZTST <= ZTST_ALWAYS && !m_context->TEST.DATE) ||
			// Depth will be written through the RT
			(context->FRAME.FBP == context->ZBUF.ZBP && !PRIM->TME && zm == 0 && fm == 0 && context->TEST.ZTE)
			);

	// Skipping draw with no color nor depth output ? 
	if (no_rt && no_ds)
		return;

	const bool draw_sprite_tex = PRIM->TME && (m_vt.m_primclass == GS_SPRITE_CLASS);
	const GSVector4 delta_p = m_vt.m_max.p - m_vt.m_min.p;
	const bool single_page = (delta_p.x <= 64.0f) && (delta_p.y <= 64.0f);

	if (m_channel_shuffle) {
		m_channel_shuffle = draw_sprite_tex && (m_context->TEX0.PSM == PSM_PSMT8) && single_page;
		if (m_channel_shuffle)
			return;
	} else if (draw_sprite_tex && GIFREG_FRAME_BLOCK(m_context->FRAME) == m_context->TEX0.TBP0) {
		// Special post-processing effect
		if ((m_context->TEX0.PSM == PSM_PSMT8) && single_page)
			m_channel_shuffle = true;
		else
			m_channel_shuffle = false;
	}
	else
		m_channel_shuffle = false;

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = GIFREG_FRAME_BLOCK(context->FRAME);
	TEX0.TBW  = context->FRAME.FBW;
	TEX0.PSM  = context->FRAME.PSM;

	GSTextureCache::Target* rt = NULL;
	GSTexture* rt_tex = NULL;
	if (!no_rt) {
		rt = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::RenderTarget, true, fm);
		rt_tex = rt->m_texture;
	}

	TEX0.TBP0 = GIFREG_ZBUF_BLOCK(context->ZBUF);
	TEX0.TBW  = context->FRAME.FBW;
	TEX0.PSM  = context->ZBUF.PSM;

	GSTextureCache::Target* ds = NULL;
	GSTexture* ds_tex = NULL;
	if (!no_ds) {
		ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, context->DepthWrite());
		ds_tex = ds->m_texture;
	}

	m_src = nullptr;
	m_texture_shuffle = false;

	if(PRIM->TME)
	{
		GIFRegCLAMP MIP_CLAMP = context->CLAMP;
		m_lod = GSVector2i(0, 0);

		// Code from the SW renderer
		if (m_mipmap && IsMipMapDraw()) {
			const int interpolation = (context->TEX1.MMIN & 1) + 1; // 1: round, 2: tri

			int k = (m_context->TEX1.K + 8) >> 4;
			int lcm = m_context->TEX1.LCM;
			const int mxl = std::min<int>((int)m_context->TEX1.MXL, 6);

			if ((int)m_vt.m_lod.x >= mxl) {
				k = mxl; // set lod to max level
				lcm = 1; // constant lod
			}

			if (PRIM->FST)
				lcm = 1;

			if (lcm == 1) {
				m_lod.x = std::max<int>(k, 0);
				m_lod.y = m_lod.x;
			} else {
				// Not constant but who care !
				if (interpolation == 2) {
					// Mipmap Linear. Both layers are sampled, only take the big one
					m_lod.x = std::max<int>((int)floor(m_vt.m_lod.x), 0);
				} else {
					// On GS lod is a fixed float number 7:4 (4 bit for the frac part)
					// Same as above with a bigger margin on rounding
					// The goal is to avoid 1 undrawn pixels around the edge which trigger the load of the big
					// layer.
					if (ceil(m_vt.m_lod.x) < m_vt.m_lod.y)
						m_lod.x = std::max<int>((int)round(m_vt.m_lod.x + 0.0625 + 0.01), 0);
					else
						m_lod.x = std::max<int>((int)round(m_vt.m_lod.x + 0.0625), 0);
				}

				m_lod.y = std::max<int>((int)ceil(m_vt.m_lod.y), 0);
			}

			m_lod.x = std::min<int>(m_lod.x, mxl);
			m_lod.y = std::min<int>(m_lod.y, mxl);

			TEX0 = GetTex0Layer(m_lod.x);

			MIP_CLAMP.MINU >>= m_lod.x;
			MIP_CLAMP.MINV >>= m_lod.x;
			MIP_CLAMP.MAXU >>= m_lod.x;
			MIP_CLAMP.MAXV >>= m_lod.x;

			for (int i = 0; i < m_lod.x; i++) {
				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;
			}
		}
		else
			TEX0 = m_context->TEX0;

		m_context->offset.tex = m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

		GSVector4i r;
		bool is_linear = m_vt.m_filter.opt_linear;
		GetTextureMinMax(r, TEX0, MIP_CLAMP, is_linear);

		m_src = tex_psm.depth ? m_tc->LookupDepthSource(TEX0, env.TEXA, r) : m_tc->LookupSource(TEX0, env.TEXA, r);

		// Round 2
		if (m_mipmap && IsMipMapDraw() && m_mipmap == 2 && !tex_psm.depth) {
			// Upload remaining texture layers
			const GSVector4 tmin = m_vt.m_min.t;
			const GSVector4 tmax = m_vt.m_max.t;

			for (int layer = m_lod.x + 1; layer <= m_lod.y; layer++) {
				const GIFRegTEX0& MIP_TEX0 = GetTex0Layer(layer);

				m_context->offset.tex = m_mem.GetOffset(MIP_TEX0.TBP0, MIP_TEX0.TBW, MIP_TEX0.PSM);

				MIP_CLAMP.MINU >>= 1;
				MIP_CLAMP.MINV >>= 1;
				MIP_CLAMP.MAXU >>= 1;
				MIP_CLAMP.MAXV >>= 1;

				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;

				GetTextureMinMax(r, MIP_TEX0, MIP_CLAMP, is_linear);

				m_src->UpdateLayer(MIP_TEX0, r, layer - m_lod.x);
			}

			m_vt.m_min.t = tmin;
			m_vt.m_max.t = tmax;
		}

		// Hypothesis: texture shuffle is used as a postprocessing effect so texture will be an old target.
		// Initially code also tested the RT but it gives too much false-positive
		//
		// Both input and output are 16 bits and texture was initially 32 bits!
		m_texture_shuffle = (GSLocalMemory::m_psm[context->FRAME.PSM].bpp == 16) && (tex_psm.bpp == 16)
			&& draw_sprite_tex && m_src->m_32_bits_fmt;

		// Okami mustn't call this code
		if (m_texture_shuffle && m_vertex.next < 3 && PRIM->FST && (m_context->FRAME.FBMSK == 0)) {
			// Avious dubious call to m_texture_shuffle on 16 bits games
			// The pattern is severals column of 8 pixels. A single sprite
			// smell fishy but a big sprite is wrong.

			// Shadow of Memories/Destiny shouldn't call this code.
			// Causes shadow flickering.
			const GSVertex* v = &m_vertex.buff[0];
			m_texture_shuffle = ((v[1].U - v[0].U) < 256) ||
				// Tomb Raider Angel of Darkness relies on this behavior to produce a fog effect.
				// In this case, the address of the framebuffer and texture are the same. 
				// The game will take RG => BA and then the BA => RG of next pixels. 
				// However, only RG => BA needs to be emulated because RG isn't used.
				GIFREG_FRAME_BLOCK(m_context->FRAME) == m_context->TEX0.TBP0 ||
				// DMC3, Onimusha 3 rely on this behavior.
				// They do fullscreen rectangle with scissor, then shift by 8 pixels, not done with recursion.
				// So we check if it's a TS effect by checking the scissor.
				((m_context->SCISSOR.SCAX1 - m_context->SCISSOR.SCAX0) < 32);
		}

		if (m_src->m_target && m_context->TEX0.PSM == PSM_PSMT8 && single_page && draw_sprite_tex)
			m_channel_shuffle = true;
		else
			m_channel_shuffle = false;
	}
	if (rt) {
		// Be sure texture shuffle detection is properly propagated
		// Otherwise set or clear the flag (Code in texture cache only set the flag)
		// Note: it is important to clear the flag when RT is used as a real 16 bits target.
		rt->m_32_bits_fmt = m_texture_shuffle || (GSLocalMemory::m_psm[context->FRAME.PSM].bpp != 16);
	}

	// The rectangle of the draw
	m_r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(context->scissor.in));

	{
		const GSVector2 up_s = GetTextureScaleFactor();
		const int up_w = static_cast<int>(std::ceil(static_cast<float>(m_r.z) * up_s.x));
		const int up_h = static_cast<int>(std::ceil(static_cast<float>(m_r.w) * up_s.y));
		const int new_w = std::max(up_w, std::max(rt_tex ? rt_tex->GetWidth() : 0, ds_tex ? ds_tex->GetWidth() : 0));
		const int new_h = std::max(up_h, std::max(rt_tex ? rt_tex->GetHeight() : 0, ds_tex ? ds_tex->GetHeight() : 0));
		std::array<GSTextureCache::Target*, 2> ts{ rt, ds };
		for (GSTextureCache::Target* t : ts)
		{
			if (t)
			{
				// Adjust texture size to fit current draw if necessary.
				GSTexture* tex = t->m_texture;
				const int w    = tex->GetWidth();
				const int h    = tex->GetHeight();
				if (w != new_w || h != new_h)
				{
					const bool is_rt = t == rt;
					t->m_texture = is_rt ?
						m_dev->CreateRenderTarget(new_w, new_h, tex->GetFormat()) :
						m_dev->CreateDepthStencil(new_w, new_h, tex->GetFormat());
					const GSVector4i r{ 0, 0, w, h };
					m_dev->CopyRect(tex, t->m_texture, r);
					m_dev->Recycle(tex);
					t->m_texture->SetScale(up_s);
					(is_rt ? rt_tex : ds_tex) = t->m_texture;
				}
			}
		}
	}

	if(m_hacks.m_oi && !(this->*m_hacks.m_oi)(rt_tex, ds_tex, m_src))
		return;

	if (!OI_BlitFMV(rt, m_src, m_r))
		return;

	if (m_userhacks_enabled_gs_mem_clear) {
		// Constant Direct Write without texture/test/blending (aka a GS mem clear)
		if ((m_vt.m_primclass == GS_SPRITE_CLASS) && !PRIM->TME // Direct write
				&& (!PRIM->ABE || m_context->ALPHA.IsOpaque()) // No transparency
				&& (m_context->FRAME.FBMSK == 0) // no color mask
				&& !m_context->TEST.ATE // no alpha test
				&& (!m_context->TEST.ZTE || m_context->TEST.ZTST == ZTST_ALWAYS) // no depth test
				&& (m_vt.m_eq.rgba == 0xFFFF) // constant color write
				&& m_r.x == 0 && m_r.y == 0) { // Likely full buffer write

			OI_GsMemClear();

			OI_DoubleHalfClear(rt_tex, ds_tex);
		}
	}

	// A couple of hack to avoid upscaling issue. So far it seems to impacts mostly sprite
	// Note: first hack corrects both position and texture coordinate
	// Note: second hack corrects only the texture coordinate
	if (CanUpscale() && (m_vt.m_primclass == GS_SPRITE_CLASS)) {
		const size_t count = m_vertex.next;
		GSVertex* v = &m_vertex.buff[0];

		// Hack to avoid vertical black line in various games (ace combat/tekken)
		if (m_userhacks_align_sprite_X)
		{
			// Note for performance reason I do the check only once on the first
			// primitive
			const int win_position = v[1].XYZ.X - context->XYOFFSET.OFX;
			const bool unaligned_position = ((win_position & 0xF) == 8);
			const bool unaligned_texture  = ((v[1].U & 0xF) == 0) && PRIM->FST; // I'm not sure this check is useful
			const bool hole_in_vertex = (count < 4) || (v[1].XYZ.X != v[2].XYZ.X);
			if (hole_in_vertex && unaligned_position && (unaligned_texture || !PRIM->FST)) {
				// Normaly vertex are aligned on full pixels and texture in half
				// pixels. Let's extend the coverage of an half-pixel to avoid
				// hole after upscaling
				for(size_t i = 0; i < count; i += 2)
				{
					v[i+1].XYZ.X += 8;
					// I really don't know if it is a good idea. Neither what to do for !PRIM->FST
					if (unaligned_texture)
						v[i+1].U += 8;
				}
			}
		}

		// Noting to do if no texture is sampled
		if (PRIM->FST && draw_sprite_tex)
		{
			bool is_linear = m_vt.m_filter.opt_linear;
			if ((m_userhacks_round_sprite_offset > 1) || (m_userhacks_round_sprite_offset == 1 && !is_linear))
			{
				if (is_linear)
					RoundSpriteOffset<true>();
				else
					RoundSpriteOffset<false>();
			}
		}
	}

	//

	DrawPrims(rt_tex, ds_tex, m_src);

	//

	context->TEST = TEST;
	context->FRAME = FRAME;
	context->ZBUF = ZBUF;

	//

	if(fm != 0xffffffff && rt)
	{
		//rt->m_valid = rt->m_valid.runion(r);
		rt->UpdateValidity(m_r);

		m_tc->InvalidateVideoMem(context->offset.fb, m_r, false);

		m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, GIFREG_FRAME_BLOCK(context->FRAME));
	}

	if(zm != 0xffffffff && ds)
	{
		//ds->m_valid = ds->m_valid.runion(r);
		ds->UpdateValidity(m_r);

		m_tc->InvalidateVideoMem(context->offset.zb, m_r, false);

		m_tc->InvalidateVideoMemType(GSTextureCache::RenderTarget, GIFREG_ZBUF_BLOCK(context->ZBUF));
	}

	if(m_hacks.m_oo)
		(this->*m_hacks.m_oo)();
}

// hacks

GSRendererHW::Hacks::Hacks()
	: m_oi_map(m_oi_list)
	, m_oo_map(m_oo_list)
	, m_cu_map(m_cu_list)
	, m_oi(NULL)
	, m_oo(NULL)
	, m_cu(NULL)
{
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::BigMuthaTruckers, CRC::RegionCount, &GSRendererHW::OI_BigMuthaTruckers));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::DBZBT2, CRC::RegionCount, &GSRendererHW::OI_DBZBTGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::DBZBT3, CRC::RegionCount, &GSRendererHW::OI_DBZBTGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFXII, CRC::EU, &GSRendererHW::OI_FFXII));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFX, CRC::RegionCount, &GSRendererHW::OI_FFX));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::MetalSlug6, CRC::RegionCount, &GSRendererHW::OI_MetalSlug6));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::RozenMaidenGebetGarden, CRC::RegionCount, &GSRendererHW::OI_RozenMaidenGebetGarden));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SonicUnleashed, CRC::RegionCount, &GSRendererHW::OI_SonicUnleashed));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::StarWarsForceUnleashed, CRC::RegionCount, &GSRendererHW::OI_StarWarsForceUnleashed));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SuperManReturns, CRC::RegionCount, &GSRendererHW::OI_SuperManReturns));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::ArTonelico2, CRC::RegionCount, &GSRendererHW::OI_ArTonelico2));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::Jak2, CRC::RegionCount, &GSRendererHW::OI_JakGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::Jak3, CRC::RegionCount, &GSRendererHW::OI_JakGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::JakX, CRC::RegionCount, &GSRendererHW::OI_JakGames));

	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::MajokkoALaMode2, CRC::RegionCount, &GSRendererHW::OO_MajokkoALaMode2));

	m_cu_list.push_back(HackEntry<CU_Ptr>(CRC::MajokkoALaMode2, CRC::RegionCount, &GSRendererHW::CU_MajokkoALaMode2));
	m_cu_list.push_back(HackEntry<CU_Ptr>(CRC::TalesOfAbyss, CRC::RegionCount, &GSRendererHW::CU_TalesOfAbyss));
}

void GSRendererHW::Hacks::SetGameCRC(const CRC::Game& game)
{
	const u32 hash = (u32)((game.region << 24) | game.title);

	m_oi = m_oi_map[hash];
	m_oo = m_oo_map[hash];
	m_cu = m_cu_map[hash];

	if (game.flags & CRC::PointListPalette)
		m_oi = &GSRendererHW::OI_PointListPalette;
}

// Trick to do a fast clear on the GS
// Set frame buffer pointer on the start of the buffer. Set depth buffer pointer on the half buffer
// FB + depth write will fill the full buffer.
void GSRendererHW::OI_DoubleHalfClear(GSTexture* rt, GSTexture* ds)
{
	// Note gs mem clear must be tested before calling this function

	// Limit further to unmask Z write
	if (!m_context->ZBUF.ZMSK && rt && ds) {
		const GSVertex* v = &m_vertex.buff[0];
		const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_context->FRAME.PSM];
		//const GSLocalMemory::psm_t& depth_psm = GSLocalMemory::m_psm[m_context->ZBUF.PSM];

		// Z and color must be constant and the same
		if (m_vt.m_eq.rgba != 0xFFFF || !m_vt.m_eq.z || v[1].XYZ.Z != v[1].RGBAQ.U32[0])
			return;

		// Format doesn't have the same size. It smells fishy (xmen...)
		//if (frame_psm.trbpp != depth_psm.trbpp)
		//	return;

		// Size of the current draw
		const u32 w_pages = static_cast<u32>(roundf(m_vt.m_max.p.x / frame_psm.pgs.x));
		const u32 h_pages = static_cast<u32>(roundf(m_vt.m_max.p.y / frame_psm.pgs.y));
		const u32 written_pages = w_pages * h_pages;

		// Frame and depth pointer can be inverted
		u32 base = 0, half = 0;

		if (m_context->FRAME.FBP > m_context->ZBUF.ZBP) {
			base = m_context->ZBUF.ZBP;
			half = m_context->FRAME.FBP;
		} else {
			base = m_context->FRAME.FBP;
			half = m_context->ZBUF.ZBP;
		}

		// If both buffers are side by side we can expect a fast clear in on-going
		if (half <= (base + written_pages)) {
			const u32 color = v[1].RGBAQ.U32[0];
			const bool clear_depth = (m_context->FRAME.FBP > m_context->ZBUF.ZBP);

			// Commit texture with a factor 2 on the height
			GSTexture* t = clear_depth ? ds : rt;

			if (clear_depth)
				// Only pure clear are supported for depth
				m_dev->ClearDepth(t);
			else
				m_dev->ClearRenderTarget(t, color);
		}
	}
}

// Note: hack is safe, but it could impact the perf a little (normally games do only a couple of clear by frame)
void GSRendererHW::OI_GsMemClear()
{
	// Note gs mem clear must be tested before calling this function

	// Limit it further to a full screen 0 write
	if ((m_vertex.next == 2) &&  m_vt.m_min.c.eq(GSVector4i(0))) {
		GSOffset* off = m_context->offset.fb;
		const GSVector4i r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(m_context->scissor.in));
		// Limit the hack to a single fullscreen clear. Some games might use severals column to clear a screen
		// but hopefully it will be enough.
		if (r.width() <= 128 || r.height() <= 128)
			return;

		const int format = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		// FIXME: loop can likely be optimized with AVX/SSE. Pixels aren't
		// linear but the value will be done for all pixels of a block.
		// FIXME: maybe we could limit the write to the top and bottom row page.
		if (format == 0) {
			// Based on WritePixel32
			for(int y = r.top; y < r.bottom; y++)
			{
				u32* RESTRICT d = &m_mem.m_vm32[off->pixel.row[y]];
				int* RESTRICT col = off->pixel.col[0];

				for(int x = r.left; x < r.right; x++)
				{
					d[col[x]] = 0; // Here the constant color
				}
			}
		} else if (format == 1) {
			// Based on WritePixel24
			for(int y = r.top; y < r.bottom; y++)
			{
				u32* RESTRICT d = &m_mem.m_vm32[off->pixel.row[y]];
				int* RESTRICT col = off->pixel.col[0];

				for(int x = r.left; x < r.right; x++)
				{
					d[col[x]] &= 0xff000000; // Clear the color
				}
			}
		}
	}
}

bool GSRendererHW::OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* tex, const GSVector4i& r_draw)
{
	if (r_draw.w > 1024 && (m_vt.m_primclass == GS_SPRITE_CLASS) && (m_vertex.next == 2) && PRIM->TME && !PRIM->ABE && tex && !tex->m_target && m_context->TEX0.TBW > 0) {
		// The draw is done past the RT at the location of the texture. To avoid various upscaling mess
		// We will blit the data from the top to the bottom of the texture manually.

		// Expected memory representation
		// -----------------------------------------------------------------
		// RT (2 half frame)
		// -----------------------------------------------------------------
		// Top of Texture (full height frame)
		//
		// Bottom of Texture (half height frame, will be the copy of Top texture after the draw)
		// -----------------------------------------------------------------

		// sRect is the top of texture
		const int tw = (int)(1 << m_context->TEX0.TW);
		const int th = (int)(1 << m_context->TEX0.TH);
		GSVector4 sRect;
		sRect.x = m_vt.m_min.t.x / tw;
		sRect.y = m_vt.m_min.t.y / th;
		sRect.z = m_vt.m_max.t.x / tw;
		sRect.w = m_vt.m_max.t.y / th;

		// Compute the Bottom of texture rectangle
		const int offset = (m_context->TEX0.TBP0 - GIFREG_FRAME_BLOCK(m_context->FRAME)) / m_context->TEX0.TBW;
		GSVector4i r_texture(r_draw);
		r_texture.y -= offset;
		r_texture.w -= offset;

		const GSVector4 dRect(r_texture);

		// Do the blit. With a Copy mess to avoid issue with limited API (dx)
		// m_dev->StretchRect(tex->m_texture, sRect, tex->m_texture, dRect);
		const GSVector4i r_full(0, 0, tw, th);
		if (GSTexture* rt = m_dev->CreateRenderTarget(tw, th)) {
			m_dev->CopyRect(tex->m_texture, rt, r_full);

			m_dev->StretchRect(tex->m_texture, sRect, rt, dRect);

			m_dev->CopyRect(rt, tex->m_texture, r_full);

			m_dev->Recycle(rt);
		}

		// Copy back the texture into the GS mem. I don't know why but it will be
		// reuploaded again later
		m_tc->Read(tex, r_texture);

		m_tc->InvalidateVideoMemSubTarget(_rt);

		return false; // skip current draw
	}

	// Nothing to see keep going
	return true;
}

// OI (others input?/implementation?) hacks replace current draw call

bool GSRendererHW::OI_BigMuthaTruckers(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Rendering pattern:
	// CRTC frontbuffer at 0x0 is interlaced (half vertical resolution),
	// game needs to do a depth effect (so green channel to alpha),
	// but there is a vram limitation so green is pushed into the alpha channel of the CRCT buffer,
	// vertical resolution is half so only half is processed at once
	// We, however, don't have this limitation so we'll replace the draw with a full-screen TS.

	const GIFRegTEX0 Texture = m_context->TEX0;

	GIFRegTEX0 Frame;
	Frame.TBW = m_context->FRAME.FBW;
	Frame.TBP0 = m_context->FRAME.FBP;
	Frame.TBP0 = GIFREG_FRAME_BLOCK(m_context->FRAME);

	if (PRIM->TME && Frame.TBW == 10 && Texture.TBW == 10 && Frame.TBP0 == 0x00a00 && Texture.PSM == PSM_PSMT8H && (m_r.y == 256 || m_r.y == 224))
	{
		// 224 ntsc, 256 pal.
		const size_t count = m_vertex.next;
		GSVertex* v = &m_vertex.buff[0];
		const u16 offset = (u16)m_r.y * 16;

		for (size_t i = 0; i < count; i++)
			v[i].V += offset;
	}

	return true;
}

bool GSRendererHW::OI_DBZBTGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (t && t->m_from_target)  // Avoid slow framebuffer readback
		return true;

	// Sprite rendering
	if (!CanUseSwSpriteRender(true))
		return true;
	
	SwSpriteRender();

	return false; // Skip current draw
}

bool GSRendererHW::OI_FFXII(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	static u32* video = NULL;
	static size_t lines = 0;

	if(lines == 0)
	{
		if(m_vt.m_primclass == GS_LINE_CLASS && (m_vertex.next == 448 * 2 || m_vertex.next == 512 * 2))
		{
			lines = m_vertex.next / 2;
		}
	}
	else
	{
		if(m_vt.m_primclass == GS_POINT_CLASS)
		{
			if(m_vertex.next >= 16 * 512)
			{
				// incoming pixels are stored in columns, one column is 16x512, total res 448x512 or 448x454

				if(!video) video = new u32[512 * 512];

				const int ox = m_context->XYOFFSET.OFX - 8;
				const int oy = m_context->XYOFFSET.OFY - 8;

				const GSVertex* RESTRICT v = m_vertex.buff;

				for(int i = (int)m_vertex.next; i > 0; i--, v++)
				{
					int x = (v->XYZ.X - ox) >> 4;
					int y = (v->XYZ.Y - oy) >> 4;

					if (x < 0 || x >= 448 || y < 0 || y >= (int)lines) return false; // le sigh

					video[(y << 8) + (y << 7) + (y << 6) + x] = v->RGBAQ.U32[0];
				}

				return false;
			}
			else
			{
				lines = 0;
			}
		}
		else if(m_vt.m_primclass == GS_LINE_CLASS)
		{
			if(m_vertex.next == lines * 2)
			{
				// normally, this step would copy the video onto screen with 512 texture mapped horizontal lines,
				// but we use the stored video data to create a new texture, and replace the lines with two triangles

				m_dev->Recycle(t->m_texture);

				t->m_texture = m_dev->CreateTexture(512, 512);

				t->m_texture->Update(GSVector4i(0, 0, 448, lines), video, 448 * 4);

				m_vertex.buff[2] = m_vertex.buff[m_vertex.next - 2];
				m_vertex.buff[3] = m_vertex.buff[m_vertex.next - 1];

				m_index.buff[0] = 0;
				m_index.buff[1] = 1;
				m_index.buff[2] = 2;
				m_index.buff[3] = 1;
				m_index.buff[4] = 2;
				m_index.buff[5] = 3;

				m_vertex.head = m_vertex.tail = m_vertex.next = 4;
				m_index.tail = 6;

				m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, GS_TRIANGLE_CLASS);
			}
			else
			{
				lines = 0;
			}
		}
	}

	return true;
}

bool GSRendererHW::OI_FFX(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	const u32 FBP = GIFREG_FRAME_BLOCK(m_context->FRAME);
	const u32 ZBP = GIFREG_ZBUF_BLOCK(m_context->ZBUF);
	const u32 TBP = m_context->TEX0.TBP0;

	// random battle transition (z buffer written directly, clear it now)
	if((FBP == 0x00d00 || FBP == 0x00000) && ZBP == 0x02100 && PRIM->TME && TBP == 0x01a00 && m_context->TEX0.PSM == PSM_PSMCT16S)
		m_dev->ClearDepth(ds);

	return true;
}

bool GSRendererHW::OI_MetalSlug6(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// missing red channel fix (looks alright in pcsx2 r5000+)

	GSVertex* RESTRICT v = m_vertex.buff;

	for(int i = (int)m_vertex.next; i > 0; i--, v++)
	{
		const u32 c = v->RGBAQ.U32[0];

		const u32 r = (c >> 0) & 0xff;
		const u32 g = (c >> 8) & 0xff;
		const u32 b = (c >> 16) & 0xff;

		if(r == 0 && g != 0 && b != 0)
		{
			v->RGBAQ.U32[0] = (c & 0xffffff00) | ((g + b + 1) >> 1);
		}
	}

	m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, m_vt.m_primclass);

	return true;
}

bool GSRendererHW::OI_RozenMaidenGebetGarden(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if(!PRIM->TME)
	{
		const u32 FBP = GIFREG_FRAME_BLOCK(m_context->FRAME);
		const u32 ZBP = GIFREG_ZBUF_BLOCK(m_context->ZBUF);

		if(FBP == 0x008c0 && ZBP == 0x01a40)
		{
			//  frame buffer clear, atst = fail, afail = write z only, z buffer points to frame buffer

			GIFRegTEX0 TEX0;

			TEX0.TBP0 = ZBP;
			TEX0.TBW = m_context->FRAME.FBW;
			TEX0.PSM = m_context->FRAME.PSM;

			if(GSTextureCache::Target* tmp_rt = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::RenderTarget, true))
				m_dev->ClearRenderTarget(tmp_rt->m_texture, 0);

			return false;
		}
		else if(FBP == 0x00000 && GIFREG_ZBUF_BLOCK(m_context->ZBUF) == 0x01180)
		{
			// z buffer clear, frame buffer now points to the z buffer (how can they be so clever?)

			GIFRegTEX0 TEX0;

			TEX0.TBP0 = FBP;
			TEX0.TBW = m_context->FRAME.FBW;
			TEX0.PSM = m_context->ZBUF.PSM;

			if(GSTextureCache::Target* tmp_ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, true))
				m_dev->ClearDepth(tmp_ds->m_texture);

			return false;
		}
	}

	return true;
}

bool GSRendererHW::OI_SonicUnleashed(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Rendering pattern is:
	// Save RG channel with a kind of a TS (replaced by a copy in this hack),
	// compute shadow in RG,
	// save result in alpha with a TS,
	// Restore RG channel that we previously copied to render shadows.

	const GIFRegTEX0 Texture = m_context->TEX0;

	GIFRegTEX0 Frame;
	Frame.TBW  = m_context->FRAME.FBW;
	Frame.TBP0 = m_context->FRAME.FBP;
	Frame.TBP0 = GIFREG_FRAME_BLOCK(m_context->FRAME);
	Frame.PSM  = m_context->FRAME.PSM;

	if ((!PRIM->TME) || (GSLocalMemory::m_psm[Texture.PSM].bpp != 16) || (GSLocalMemory::m_psm[Frame.PSM].bpp != 16))
		return true;

	if ((Texture.TBP0 == Frame.TBP0) || (Frame.TBW != 16 && Texture.TBW != 16))
		return true;

	GSTextureCache::Target* src = m_tc->LookupTarget(Texture, m_width, m_height, GSTextureCache::RenderTarget, true);

	const GSVector2i size = rt->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, size.x, size.y);

	m_dev->StretchRect(src->m_texture, sRect, rt, dRect, true, true, true, false);

	return false;
}

bool GSRendererHW::OI_StarWarsForceUnleashed(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	const u32 FBP  = GIFREG_FRAME_BLOCK(m_context->FRAME);
	const u32 FPSM = m_context->FRAME.PSM;

	if(PRIM->TME)
	{
		if((FBP == 0x0 || FBP == 0x01180) && FPSM == PSM_PSMCT32 && (m_vt.m_eq.z && m_vt.m_max.p.z == 0))
			m_dev->ClearDepth(ds);
	}

	return true;
}

bool GSRendererHW::OI_PointListPalette(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if(m_vt.m_primclass == GS_POINT_CLASS && !PRIM->TME)
	{
		u32 FBP = GIFREG_FRAME_BLOCK(m_context->FRAME);
		u32 FBW = m_context->FRAME.FBW;

		if(FBP >= 0x03f40 && (FBP & 0x1f) == 0)
		{
			if(m_vertex.next == 16)
			{
				GSVertex* RESTRICT v = m_vertex.buff;

				for(int i = 0; i < 16; i++, v++)
				{
					u32 c = v->RGBAQ.U32[0];
					const u32 a = c >> 24;

					c = (a >= 0x80 ? 0xff000000 : (a << 25)) | (c & 0x00ffffff);

					v->RGBAQ.U32[0] = c;

					m_mem.WritePixel32(i & 7, i >> 3, c, FBP, FBW);
				}

				m_mem.m_clut.Invalidate();

				return false;
			}
			else if(m_vertex.next == 256)
			{
				GSVertex* RESTRICT v = m_vertex.buff;

				for(int i = 0; i < 256; i++, v++)
				{
					u32 c = v->RGBAQ.U32[0];
					const u32 a = c >> 24;

					c = (a >= 0x80 ? 0xff000000 : (a << 25)) | (c & 0x00ffffff);

					v->RGBAQ.U32[0] = c;

					m_mem.WritePixel32(i & 15, i >> 4, c, FBP, FBW);
				}

				m_mem.m_clut.Invalidate();

				return false;
			}
		}
	}

	return true;
}

bool GSRendererHW::OI_SuperManReturns(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Instead to use a fullscreen rectangle they use a 32 pixels, 4096 pixels with a FBW of 1.
	// Technically the FB wrap/overlap on itself...
	const GSDrawingContext* ctx = m_context;

	if (!(ctx->FRAME.FBP == ctx->ZBUF.ZBP && !PRIM->TME && !ctx->ZBUF.ZMSK && !ctx->FRAME.FBMSK && m_vt.m_eq.rgba == 0xFFFF))
		return true;

	// Do a direct write
	m_dev->ClearRenderTarget(rt, GSVector4(m_vt.m_min.c));

	m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, GIFREG_FRAME_BLOCK(ctx->FRAME));

	return false;
}

bool GSRendererHW::OI_ArTonelico2(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// world map clipping
	//
	// The bad draw call is a sprite rendering to clear the z buffer

	/*
	   Depth buffer description
	   * width is 10 pages
	   * texture/scissor size is 640x448
	   * depth is 16 bits so it writes 70 (10w * 7h) pages of data.

	   following draw calls will use the buffer as 6 pages width with a scissor
	   test of 384x672. So the above texture can be seen as a

	   * texture width: 6 pages * 64 pixels/page = 384
	   * texture height: 70/6 pages * 64 pixels/page =746

	   So as you can see the GS issue a write of 640x448 but actually it
	   expects to clean a 384x746 area. Ideally the fix will transform the
	   buffer to adapt the page width properly.
	 */

	const GSVertex* v = &m_vertex.buff[0];

	if (m_vertex.next == 2 && !PRIM->TME && m_context->FRAME.FBW == 10 && v->XYZ.Z == 0 && m_context->TEST.ZTST == ZTST_ALWAYS)
		m_dev->ClearDepth(ds);

	return true;
}

bool GSRendererHW::OI_JakGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (!CanUseSwSpriteRender(false))
		return true;

	// Render 16x16 palette via CPU.
	SwSpriteRender();

	return false;  // Skip current draw.
}

// OO (others output?) hacks: invalidate extra local memory after the draw call

void GSRendererHW::OO_MajokkoALaMode2()
{
	// palette readback

	const u32 FBP = GIFREG_FRAME_BLOCK(m_context->FRAME);

	if(!PRIM->TME && FBP == 0x03f40)
	{
		GIFRegBITBLTBUF BITBLTBUF;

		BITBLTBUF.SBP = FBP;
		BITBLTBUF.SBW = 1;
		BITBLTBUF.SPSM = PSM_PSMCT32;

		InvalidateLocalMem(BITBLTBUF, GSVector4i(0, 0, 16, 16));
	}
}

// Can Upscale hacks: disable upscaling for some draw calls

bool GSRendererHW::CU_MajokkoALaMode2()
{
	// palette should stay 16 x 16

	const u32 FBP = GIFREG_FRAME_BLOCK(m_context->FRAME);

	return FBP != 0x03f40;
}

bool GSRendererHW::CU_TalesOfAbyss()
{
	// full image blur and brightening

	const u32 FBP = GIFREG_FRAME_BLOCK(m_context->FRAME);

	return FBP != 0x036e0 && FBP != 0x03560 && FBP != 0x038e0;
}
