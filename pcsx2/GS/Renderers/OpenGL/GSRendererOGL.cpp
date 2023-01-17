/*
 *	Copyright (C) 2011-2011 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
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

#include "GSRendererOGL.h"
#include "options_tools.h"

GSRendererOGL::GSRendererOGL()
	: GSRendererHW(new GSTextureCacheOGL(this))
{
	m_sw_blending        = option_value(INT_PCSX2_OPT_BLEND_UNIT_ACCURACY, KeyOptionInt::return_type);
	UserHacks_tri_filter = static_cast<TriFiltering>(theApp.GetConfigI("UserHacks_TriFilter"));

	// Hope nothing requires too many draw calls.
	m_drawlist.reserve(2048);

	m_prim_overlap = PRIM_OVERLAP_UNKNOW;
	ResetStates();
}

void GSRendererOGL::SetupIA(const float& sx, const float& sy)
{
	GSDeviceOGL* dev = (GSDeviceOGL*)m_dev;


	if (m_userhacks_wildhack && !m_isPackedUV_HackFlag && PRIM->TME && PRIM->FST) {
		for(unsigned int i = 0; i < m_vertex.next; i++)
			m_vertex.buff[i].UV &= 0x3FEF3FEF;
	}

	GLenum t = 0;
	const bool unscale_pt_ln = m_userHacks_enabled_unscale_ptln && (GetUpscaleMultiplier() != 1) && GLLoader::found_geometry_shader;

	switch(m_vt.m_primclass)
	{
		case GS_POINT_CLASS:
			if (unscale_pt_ln) {
				m_gs_sel.point = 1;
				vs_cb.PointSize = GSVector2(16.0f * sx, 16.0f * sy);
			}

			t = GL_POINTS;
			break;

		case GS_LINE_CLASS:
			if (unscale_pt_ln) {
				m_gs_sel.line = 1;
				vs_cb.PointSize = GSVector2(16.0f * sx, 16.0f * sy);
			}

			t = GL_LINES;
			break;

		case GS_SPRITE_CLASS:
			// Heuristics: trade-off
			// Lines: GPU conversion => ofc, more GPU. And also more CPU due to extra shader validation stage.
			// Triangles: CPU conversion => ofc, more CPU ;) more bandwidth (72 bytes / sprite)
			//
			// Note: severals openGL operation does draw call under the wood like texture upload. So even if
			// you do 10 consecutive draw with the geometry shader, you will still pay extra validation if new
			// texture are uploaded. (game Shadow Hearts)
			//
			// Note2: Due to MultiThreaded driver, Nvidia suffers less of the previous issue. Still it isn't free
			// Shadow Heart is 90 fps (gs) vs 113 fps (no gs)
			//
			// Note3: Some GPUs (Happens on GT 750m, not on Intel 5200) don't properly divide by large floats (e.g. FLT_MAX/FLT_MAX == 0)
			// Lines2Sprites predivides by Q, avoiding this issue, so always use it if m_vt.m_accurate_stq

			// If the draw calls contains few primitives. Geometry Shader gain with be rather small versus
			// the extra validation cost of the extra stage.
			//
			// Note: keep Geometry Shader in the replayer to ease debug.
			if (GLLoader::found_geometry_shader && !m_vt.m_accurate_stq && (m_vertex.next > 32)) { // <=> 16 sprites (based on Shadow Hearts)
				m_gs_sel.sprite = 1;

				t = GL_LINES;
			} else {
				Lines2Sprites();

				t = GL_TRIANGLES;
			}
			break;

		case GS_TRIANGLE_CLASS:
			t = GL_TRIANGLES;
			break;

		default:
			break;
	}

	dev->IASetVertexBuffer(m_vertex.buff, m_vertex.next);
	dev->IASetIndexBuffer(m_index.buff, m_index.tail);
	dev->IASetPrimitiveTopology(t);
}

void GSRendererOGL::EmulateAtst(const int pass, const GSTextureCache::Source* tex)
{
	static const u32 inverted_atst[] = {ATST_ALWAYS, ATST_NEVER, ATST_GEQUAL, ATST_GREATER, ATST_NOTEQUAL, ATST_LESS, ATST_LEQUAL, ATST_EQUAL};
	int atst = (pass == 2) ? inverted_atst[m_context->TEST.ATST] : m_context->TEST.ATST;

	if (!m_context->TEST.ATE) return;

	switch (atst) {
		case ATST_LESS:
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f;
			m_ps_sel.atst = 1;
			break;
		case ATST_LEQUAL:
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f + 1.0f;
			m_ps_sel.atst = 1;
			break;
		case ATST_GEQUAL:
			// Maybe a -1 trick multiplication factor could be used to merge with ATST_LEQUAL case
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f;
			m_ps_sel.atst = 2;
			break;
		case ATST_GREATER:
			// Maybe a -1 trick multiplication factor could be used to merge with ATST_LESS case
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f + 1.0f;
			m_ps_sel.atst = 2;
			break;
		case ATST_EQUAL:
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF;
			m_ps_sel.atst = 3;
			break;
		case ATST_NOTEQUAL:
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF;
			m_ps_sel.atst = 4;
			break;

		case ATST_NEVER: // Draw won't be done so no need to implement it in shader
		case ATST_ALWAYS:
		default:
			m_ps_sel.atst = 0;
			break;
	}
}

void GSRendererOGL::EmulateZbuffer()
{
	if (m_context->TEST.ZTE)
	{
		m_om_dssel.ztst = m_context->TEST.ZTST;
		// AA1: Z is not written on lines since coverage is always less than 0x80.
		m_om_dssel.zwe = (m_context->ZBUF.ZMSK || (PRIM->AA1 && m_vt.m_primclass == GS_LINE_CLASS)) ? 0 : 1;
	}
	else
		m_om_dssel.ztst = ZTST_ALWAYS;

	// On the real GS we appear to do clamping on the max z value the format allows.
	// Clamping is done after rasterization.
	const u32 max_z = 0xFFFFFFFF >> (GSLocalMemory::m_psm[m_context->ZBUF.PSM].fmt * 8);
	const bool clamp_z = (u32)(GSVector4i(m_vt.m_max.p).z) > max_z;

	vs_cb.MaxDepth = GSVector2i(0xFFFFFFFF);
	//ps_cb.MaxDepth = GSVector4(0.0f, 0.0f, 0.0f, 1.0f);
	m_ps_sel.zclamp = 0;

	if (clamp_z) {
		if (m_vt.m_primclass == GS_SPRITE_CLASS || m_vt.m_primclass == GS_POINT_CLASS) {
			vs_cb.MaxDepth = GSVector2i(max_z);
		} else if (!m_context->ZBUF.ZMSK) {
			ps_cb.MaxDepth = GSVector4(0.0f, 0.0f, 0.0f, max_z * ldexpf(1, -32));
			m_ps_sel.zclamp = 1;
		}
	}

	const GSVertex* v = &m_vertex.buff[0];
	// Minor optimization of a corner case (it allow to better emulate some alpha test effects)
	if (m_om_dssel.ztst == ZTST_GEQUAL && m_vt.m_eq.z && v[0].XYZ.Z == max_z)
		m_om_dssel.ztst = ZTST_ALWAYS;
}

void GSRendererOGL::EmulateTextureShuffleAndFbmask()
{
	// Uncomment to disable texture shuffle emulation.
	// m_texture_shuffle = false;

	if (m_texture_shuffle) {
		m_ps_sel.shuffle = 1;
		m_ps_sel.dfmt = 0;

		bool write_ba;
		bool read_ba;

		ConvertSpriteTextureShuffle(write_ba, read_ba);

		// If date is enabled you need to test the green channel instead of the
		// alpha channel. Only enable this code in DATE mode to reduce the number
		// of shader.
		m_ps_sel.write_rg = !write_ba && m_context->TEST.DATE;

		m_ps_sel.read_ba = read_ba;

		// Please bang my head against the wall!
		// 1/ Reduce the frame mask to a 16 bit format
		const u32& m = m_context->FRAME.FBMSK;
		const u32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
		// FIXME GSVector will be nice here
		const u8 rg_mask = fbmask & 0xFF;
		const u8 ba_mask = (fbmask >> 8) & 0xFF;
		m_om_csel.wrgba = 0;

		// 2 Select the new mask (Please someone put SSE here)
		if (rg_mask != 0xFF) {
			if (write_ba)
				m_om_csel.wb = 1;
			else
				m_om_csel.wr = 1;
			if (rg_mask)
				m_ps_sel.fbmask = 1;
		}

		if (ba_mask != 0xFF) {
			if (write_ba)
				m_om_csel.wa = 1;
			else
				m_om_csel.wg = 1;
			if (ba_mask)
				m_ps_sel.fbmask = 1;
		}

		if (m_ps_sel.fbmask && m_sw_blending)
		{
			ps_cb.FbMask.r = rg_mask;
			ps_cb.FbMask.g = rg_mask;
			ps_cb.FbMask.b = ba_mask;
			ps_cb.FbMask.a = ba_mask;

			// No blending so hit unsafe path.
			if (!PRIM->ABE)
				m_require_one_barrier = true;
			else
				m_require_full_barrier = true;
		}
		else
			m_ps_sel.fbmask = 0;

	} else {
		m_ps_sel.dfmt = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		const GSVector4i fbmask_v = GSVector4i::load((int)m_context->FRAME.FBMSK);
		const int ff_fbmask = fbmask_v.eq8(GSVector4i::xffffffff()).mask();
		const int zero_fbmask = fbmask_v.eq8(GSVector4i::zero()).mask();

		m_om_csel.wrgba = ~ff_fbmask; // Enable channel if at least 1 bit is 0

		m_ps_sel.fbmask = m_sw_blending && (~ff_fbmask & ~zero_fbmask & 0xF);

		if (m_ps_sel.fbmask) {
			ps_cb.FbMask = fbmask_v.u8to32();
			// Only alpha is special here, I think we can take a very unsafe shortcut
			// Alpha isn't blended on the GS but directly copyied into the RT.
			//
			// Behavior is clearly undefined however there is a high probability that
			// it will work. Masked bit will be constant and normally the same everywhere
			// RT/FS output/Cached value.
			//
			// Just to be sure let's add a new safe hack for unsafe access :)
			//
			// Here the GL spec quote to emphasize the unexpected behavior.
			/*
			   - If a texel has been written, then in order to safely read the result
			   a texel fetch must be in a subsequent Draw separated by the command

			   void TextureBarrier(void);

			   TextureBarrier() will guarantee that writes have completed and caches
			   have been invalidated before subsequent Draws are executed.
			 */
			// No blending so hit unsafe path.
			if (!PRIM->ABE || !(~ff_fbmask & ~zero_fbmask & 0x7)) {
				m_require_one_barrier = true;
			} else {
				// The safe and accurate path (but slow)
				m_require_full_barrier = true;
			}
		}
	}
}

void GSRendererOGL::EmulateChannelShuffle(GSTexture** rt, const GSTextureCache::Source* tex)
{
	GSDeviceOGL* dev         = (GSDeviceOGL*)m_dev;

	// Uncomment to disable HLE emulation (allow to trace the draw call)
	// m_channel_shuffle = false;

	// First let's check we really have a channel shuffle effect
	if (m_channel_shuffle) {
		if (m_game.title == CRC::GT4 || m_game.title == CRC::GT3 || m_game.title == CRC::GTConcept || m_game.title == CRC::TouristTrophy) {
			// Gran Turismo RGB channel
			m_ps_sel.channel = ChannelFetch_RGB;
			m_context->TEX0.TFX = TFX_DECAL;
			*rt = tex->m_from_target;
		} else if (m_game.title == CRC::Tekken5) {
			if (m_context->FRAME.FBW == 1) {
				// Used in stages: Secret Garden, Acid Rain, Moonlit Wilderness
				// Tekken 5 RGB channel
				m_ps_sel.channel = ChannelFetch_RGB;
				m_context->FRAME.FBMSK = 0xFF000000;
				// 12 pages: 2 calls by channel, 3 channels, 1 blit
				// Minus current draw call
				m_skip = 12 * (3 + 3 + 1) - 1;
				*rt = tex->m_from_target;
			} else {
				// Could skip model drawing if wrongly detected
				m_channel_shuffle = false;
			}
		} else if ((tex->m_texture->GetType() == GSTexture::DepthStencil) && !(tex->m_32_bits_fmt)) {
			// So far 2 games hit this code path. Urban Chaos and Tales of Abyss
			// UC: will copy depth to green channel
			// ToA: will copy depth to alpha channel
			if ((m_context->FRAME.FBMSK & 0xFF0000) == 0xFF0000) {
				// Green channel is masked
				// Tales of Abyss Craziness (MSB 16b Depth to Alpha 
				m_ps_sel.tales_of_abyss_hle = 1;
			} else {
				// Urban Chaos craziness (Green extraction)
				m_ps_sel.urban_chaos_hle = 1;
			}
		} else if (m_index.tail <= 64 && m_context->CLAMP.WMT == 3) {
			// Blood will tell. I think it is channel effect too but again
			// implemented in a different way. I don't want to add more CRC stuff. So
			// let's disable channel when the signature is different
			//
			// Note: Tales Of Abyss and Tekken5 could hit this path too. Those games are
			// handled above.
			// Maybe not a channel!
			m_channel_shuffle = false;
		} else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MAXU & 0x8) == 8)) {
			// Read either blue or Alpha. Let's go for Blue ;)
			// MGS3/Kill Zone
			// Blue channel
			m_ps_sel.channel = ChannelFetch_BLUE;
		} else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MINU & 0x8) == 0)) {
			// Read either Red or Green. Let's check the V coordinate. 0-1 is likely top so
			// red. 2-3 is likely bottom so green (actually depends on texture base pointer offset)
			const bool green = PRIM->FST && (m_vertex.buff[0].V & 32);
			if (green && (m_context->FRAME.FBMSK & 0x00FFFFFF) == 0x00FFFFFF) {
				// Typically used in Terminator 3
				const int blue_mask  = m_context->FRAME.FBMSK >> 24;
				int blue_shift = -1;

				// Note: potentially we could also check the value of the clut
				switch (blue_mask) {
					case 0xFE: blue_shift = 1; break;
					case 0xFC: blue_shift = 2; break;
					case 0xF8: blue_shift = 3; break;
					case 0xF0: blue_shift = 4; break;
					case 0xE0: blue_shift = 5; break;
					case 0xC0: blue_shift = 6; break;
					case 0x80: blue_shift = 7; break;
					case 0xFF:
					default:                   break;
				}


				if (blue_shift >= 0) {
					// Green/blue channel
					const int green_mask  = ~blue_mask & 0xFF;
					const int green_shift = 8 - blue_shift;
					dev->SetupCBMisc(GSVector4i(blue_mask, blue_shift, green_mask, green_shift));
					m_ps_sel.channel = ChannelFetch_GXBY;
					m_context->FRAME.FBMSK = 0x00FFFFFF;
				} else {
					// Green channel (wrong mask)
					m_ps_sel.channel = ChannelFetch_GREEN;
				}

			} else if (green) {
				// Green channel
				m_ps_sel.channel = ChannelFetch_GREEN;
			} else {
				// Pop
				// Red channel
				m_ps_sel.channel = ChannelFetch_RED;
			}
		} else {
			// Channel not supported
			m_channel_shuffle = false;
		}
	}

	// Effect is really a channel shuffle effect so let's cheat a little
	if (m_channel_shuffle) {
		dev->PSSetShaderResource(4, tex->m_from_target);
		m_require_one_barrier = true;

		// Replace current draw with a fullscreen sprite
		//
		// Performance GPU note: it could be wise to reduce the size to
		// the rendered size of the framebuffer

		GSVertex* s = &m_vertex.buff[0];
		s[0].XYZ.X = (u16)(m_context->XYOFFSET.OFX + 0);
		s[1].XYZ.X = (u16)(m_context->XYOFFSET.OFX + 16384);
		s[0].XYZ.Y = (u16)(m_context->XYOFFSET.OFY + 0);
		s[1].XYZ.Y = (u16)(m_context->XYOFFSET.OFY + 16384);

		m_vertex.head = m_vertex.tail = m_vertex.next = 2;
		m_index.tail = 2;

	}
}

void GSRendererOGL::EmulateBlending(bool& DATE_GL42, bool& DATE_GL45)
{
	GSDeviceOGL* dev         = (GSDeviceOGL*)m_dev;

	// AA1: Don't enable blending on AA1, not yet implemented on hardware mode,
	// it requires coverage sample so it's safer to turn it off instead.
	const bool aa1 = PRIM->AA1 && (m_vt.m_primclass == GS_LINE_CLASS);

	// No blending so early exit
	if (!aa1 && !(PRIM->ABE || m_env.PABE.PABE))
	{
		dev->OMSetBlendState();
		return;
	}

	// Compute the blending equation to detect special case
	const GIFRegALPHA& ALPHA = m_context->ALPHA;
	const u8 blend_index  = u8(((ALPHA.A * 3 + ALPHA.B) * 3 + ALPHA.C) * 3 + ALPHA.D);
	const int blend_flag = m_dev->GetBlendFlags(blend_index);

	// Do the multiplication in shader for blending accumulation: Cs*As + Cd or Cs*Af + Cd
	bool accumulation_blend = !!(blend_flag & BLEND_ACCU);

	// Blending doesn't require barrier, or sampling of the rt
	const bool blend_non_recursive = !!(blend_flag & BLEND_NO_REC);

	// BLEND MIX selection, use a mix of hw/sw blending
	if (!m_vt.m_alpha.valid && (ALPHA.C == 0))
		GetAlphaMinMax();
	const bool blend_mix1 = !!(blend_flag & BLEND_MIX1);
	const bool blend_mix2 = !!(blend_flag & BLEND_MIX2);
	const bool blend_mix3 = !!(blend_flag & BLEND_MIX3);
	bool blend_mix  = (blend_mix1 || blend_mix2 || blend_mix3)
		// Do not enable if As > 128 or F > 128, hw blend clamps to 1
		&& !((ALPHA.C == 0 && m_vt.m_alpha.max > 128) || (ALPHA.C == 2 && ALPHA.FIX > 128u));

	// SW Blend is (nearly) free. Let's use it.
	const bool impossible_or_free_blend = (blend_flag & BLEND_A_MAX) // Impossible blending
		|| blend_non_recursive                 // Free sw blending, doesn't require barriers or reading fb
		|| accumulation_blend                  // Mix of hw/sw blending
		|| (m_prim_overlap == PRIM_OVERLAP_NO) // Blend can be done in a single draw
		|| (m_require_full_barrier);           // Another effect (for example fbmask) already requires a full barrier

	// Warning no break on purpose
	// Note: the "fall through" comments tell gcc not to complain about not having breaks.
	bool sw_blending         = false;
	switch (m_sw_blending) {
		case ACC_BLEND_ULTRA:
			sw_blending |= true;
			// fall through
		case ACC_BLEND_FULL:
			sw_blending |= (ALPHA.A != ALPHA.B) && ((ALPHA.C == 0 && m_vt.m_alpha.max > 128) || (ALPHA.C == 2 && ALPHA.FIX > 128u));
			// fall through
		case ACC_BLEND_HIGH:
			sw_blending |= (ALPHA.C == 1);
			// fall through
		case ACC_BLEND_MEDIUM:
			// Initial idea was to enable accurate blending for sprite rendering to handle
			// correctly post-processing effect. Some games (ZoE) use tons of sprites as particles.
			// In order to keep it fast, let's limit it to smaller draw call.
			sw_blending |= m_vt.m_primclass == GS_SPRITE_CLASS && m_drawlist.size() < 100;
			// fall through
		case ACC_BLEND_BASIC:
			sw_blending |= impossible_or_free_blend;
			// fall through
		default:
			/*sw_blending |= accumulation_blend*/;
	}

	// Do not run BLEND MIX if sw blending is already present, it's less accurate
	if (m_sw_blending)
	{
		blend_mix   &= !sw_blending;
		sw_blending |=  blend_mix;
	}

	// Color clip
	if (m_env.COLCLAMP.CLAMP == 0) {
		// Safe FBMASK, avoid hitting accumulation mode on 16bit,
		// fixes shadows in Superman shadows of Apokolips.
		const bool sw_fbmask_colclip = !m_require_one_barrier && m_ps_sel.fbmask;
		const bool free_colclip = m_prim_overlap == PRIM_OVERLAP_NO || blend_non_recursive || sw_fbmask_colclip;
		if (free_colclip) {
			// The fastest algo that requires a single pass
			// COLCLIP Free mode ENABLED
			m_ps_sel.colclip = 1;
			sw_blending = true;
			accumulation_blend = false; // disable the HDR algo
			blend_mix = false;
		} else if (accumulation_blend || blend_mix)
		{
			// A fast algo that requires 2 passes
			// COLCLIP Fast HDR mode ENABLED
			m_ps_sel.hdr = 1;
			sw_blending  = true; // Enable sw blending for the HDR algo
		} else if (sw_blending) {
			// A slow algo that could requires several passes (barely used)
			// COLCLIP SW mode ENABLED
			m_ps_sel.colclip = 1;
		} else {
			// COLCLIP HDR mode ENABLED
			m_ps_sel.hdr = 1;
		}
	}

	// Per pixel alpha blending
	if (m_env.PABE.PABE)
	{
		// Breath of Fire Dragon Quarter, Strawberry Shortcake, Super Robot Wars, Cartoon Network Racing.
		if (sw_blending)
		{
			// PABE mode ENABLED
			m_ps_sel.pabe = 1;
			accumulation_blend = false;
			blend_mix = false;
		}
	}


	// GL42 interact very badly with sw blending. GL42 uses the primitiveID to find the primitive
	// that write the bad alpha value. Sw blending will force the draw to run primitive by primitive
	// (therefore primitiveID will be constant to 1).
	// Switch DATE_GL42 with DATE_GL45 in such cases to ensure accuracy.
	// No mix of COLCLIP + sw blend + DATE_GL42, neither sw fbmask + DATE_GL42.
	// Note: Do the swap after colclip to avoid adding extra conditions.
	if (sw_blending && DATE_GL42) {
		m_require_full_barrier = true;
		DATE_GL42 = false;
		DATE_GL45 = true;
	}

	// For stat to optimize accurate option
	if (sw_blending) {
		m_ps_sel.blend_a = ALPHA.A;
		m_ps_sel.blend_b = ALPHA.B;
		m_ps_sel.blend_c = ALPHA.C;
		m_ps_sel.blend_d = ALPHA.D;

		if (accumulation_blend) {
			// Keep HW blending to do the addition/subtraction
			dev->OMSetBlendState(blend_index, 0, false, true);
			if (ALPHA.A == 2) {
				// The blend unit does a reverse subtraction so it means
				// the shader must output a positive value.
				// Replace 0 - Cs by Cs - 0
				m_ps_sel.blend_a = ALPHA.B;
				m_ps_sel.blend_b = 2;
			}
			// Remove the addition/substraction from the SW blending
			m_ps_sel.blend_d = 2;

			// Note accumulation_blend doesn't require a barrier

		}
		else if (blend_mix)
		{
			dev->OMSetBlendState(blend_index, ALPHA.FIX, ALPHA.C == 2, false, true);

			if (blend_mix1)
			{
				m_ps_sel.blend_a = 0;
				m_ps_sel.blend_b = 2;
				m_ps_sel.blend_d = 2;
			}
			else if (blend_mix2)
			{
				m_ps_sel.blend_a = 0;
				m_ps_sel.blend_b = 2;
				m_ps_sel.blend_d = 0;
			}
			else if (blend_mix3)
			{
				m_ps_sel.blend_a = 2;
				m_ps_sel.blend_b = 0;
				m_ps_sel.blend_d = 0;
			}
		}
		else
		{
			// Disable HW blending
			dev->OMSetBlendState();

			m_require_full_barrier |= !blend_non_recursive;
		}

		// Require the fix alpha vlaue
		if (ALPHA.C == 2) {
			ps_cb.TA_Af.a = (float)ALPHA.FIX / 128.0f;
		}
	} else {
		m_ps_sel.clr1 = !!(blend_flag & BLEND_C_CLR);
		if (m_ps_sel.dfmt == 1 && ALPHA.C == 1) {
			// 24 bits doesn't have an alpha channel so use 1.0f fix factor as equivalent
			const u8 hacked_blend_index  = blend_index + 3; // +3 <=> +1 on C
			dev->OMSetBlendState(hacked_blend_index, 128, true);
		} else {
			dev->OMSetBlendState(blend_index, ALPHA.FIX, (ALPHA.C == 2));
		}
	}
}

void GSRendererOGL::EmulateTextureSampler(const GSTextureCache::Source* tex)
{
	GSDeviceOGL* dev         = (GSDeviceOGL*)m_dev;

	// Warning fetch the texture PSM format rather than the context format. The latter could have been corrected in the texture cache for depth.
	//const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];
	const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[tex->m_TEX0.PSM];
	const GSLocalMemory::psm_t &cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[m_context->TEX0.CPSM] : psm;

	const u8 wms = m_context->CLAMP.WMS;
	const u8 wmt = m_context->CLAMP.WMT;
	const bool complex_wms_wmt = !!((wms | wmt) & 2);

	const bool need_mipmap = IsMipMapDraw();
	const bool shader_emulated_sampler = tex->m_palette || cpsm.fmt != 0 || complex_wms_wmt || psm.depth;
	const bool trilinear_manual = need_mipmap && m_mipmap == 2;

	bool bilinear = m_vt.m_filter.opt_linear;
	int trilinear = 0;
	bool trilinear_auto = false;
	switch (UserHacks_tri_filter)
	{
		case TriFiltering::Forced:
			trilinear = static_cast<u8>(GS_MIN_FILTER::Linear_Mipmap_Linear);
			trilinear_auto = m_mipmap != 2;
			break;

		case TriFiltering::PS2:
			if (need_mipmap && m_mipmap != 2) {
				trilinear = m_context->TEX1.MMIN;
				trilinear_auto = true;
			}
			break;

		case TriFiltering::None:
		default:
			break;
	}

	// 1 and 0 are equivalent
	m_ps_sel.wms = (wms & 2) ? wms : 0;
	m_ps_sel.wmt = (wmt & 2) ? wmt : 0;

	// Performance note:
	// 1/ Don't set 0 as it is the default value
	// 2/ Only keep aem when it is useful (avoid useless shader permutation)
	if (m_ps_sel.shuffle) {
		// Force a 32 bits access (normally shuffle is done on 16 bits)
		// m_ps_sel.tex_fmt = 0; // removed as an optimization
		m_ps_sel.aem     = m_env.TEXA.AEM;

		// Require a float conversion if the texure is a depth otherwise uses Integral scaling
		if (psm.depth) {
			m_ps_sel.depth_fmt = (tex->m_texture->GetType() != GSTexture::DepthStencil) ? 3 : 1;
			m_vs_sel.int_fst = !PRIM->FST; // select float/int coordinate
		}

		// Shuffle is a 16 bits format, so aem is always required
		GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());
		ta /= 255.0f;
		// FIXME rely on compiler for the optimization
		ps_cb.TA_Af.x = ta.x;
		ps_cb.TA_Af.y = ta.y;

		// The purpose of texture shuffle is to move color channel. Extra interpolation is likely a bad idea.
		bilinear &= m_vt.m_filter.opt_linear;

		vs_cb.TextureOffset = RealignTargetTextureCoordinate(tex);

	} else if (tex->m_target) {
		// Use an old target. AEM and index aren't resolved it must be done
		// on the GPU

		// Select the 32/24/16 bits color (AEM)
		m_ps_sel.tex_fmt = cpsm.fmt;
		m_ps_sel.aem     = m_env.TEXA.AEM;

		// Don't upload AEM if format is 32 bits
		if (cpsm.fmt) {
			GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());
			ta /= 255.0f;
			// FIXME rely on compiler for the optimization
			ps_cb.TA_Af.x = ta.x;
			ps_cb.TA_Af.y = ta.y;
		}

		// Select the index format
		if (tex->m_palette) {
			// FIXME Potentially improve fmt field in GSLocalMemory
			if (m_context->TEX0.PSM == PSM_PSMT4HL)
				m_ps_sel.tex_fmt |= 1 << 2;
			else if (m_context->TEX0.PSM == PSM_PSMT4HH)
				m_ps_sel.tex_fmt |= 2 << 2;
			else
				m_ps_sel.tex_fmt |= 3 << 2;

			// Alpha channel of the RT is reinterpreted as an index. Star
			// Ocean 3 uses it to emulate a stencil buffer.  It is a very
			// bad idea to force bilinear filtering on it.
			bilinear &= m_vt.m_filter.opt_linear;
		}

		// Depth format
		if (tex->m_texture->GetType() == GSTexture::DepthStencil) {
			// Require a float conversion if the texure is a depth format
			m_ps_sel.depth_fmt = (psm.bpp == 16) ? 2 : 1;
			m_vs_sel.int_fst = !PRIM->FST; // select float/int coordinate

			// Don't force interpolation on depth format
			bilinear &= m_vt.m_filter.opt_linear;
		} else if (psm.depth) {
			// Use Integral scaling
			m_ps_sel.depth_fmt = 3;
			m_vs_sel.int_fst = !PRIM->FST; // select float/int coordinate

			// Don't force interpolation on depth format
			bilinear &= m_vt.m_filter.opt_linear;
		}

		vs_cb.TextureOffset = RealignTargetTextureCoordinate(tex);

	} else if (tex->m_palette) {
		// Use a standard 8 bits texture. AEM is already done on the CLUT
		// Therefore you only need to set the index
		// m_ps_sel.aem     = 0; // removed as an optimization

		// Note 4 bits indexes are converted to 8 bits
		m_ps_sel.tex_fmt = 3 << 2;

	}

	if (m_context->TEX0.TFX == TFX_MODULATE && m_vt.m_eq.rgba == 0xFFFF && m_vt.m_min.c.eq(GSVector4i(128))) {
		// Micro optimization that reduces GPU load (removes 5 instructions on the FS program)
		m_ps_sel.tfx = TFX_DECAL;
	} else {
		m_ps_sel.tfx = m_context->TEX0.TFX;
	}

	m_ps_sel.tcc = m_context->TEX0.TCC;

	m_ps_sel.ltf = bilinear && shader_emulated_sampler;
	m_ps_sel.point_sampler = GLLoader::vendor_id_amd && (!bilinear || shader_emulated_sampler);

	const int w = tex->m_texture->GetWidth();
	const int h = tex->m_texture->GetHeight();

	const int tw = (int)(1 << m_context->TEX0.TW);
	const int th = (int)(1 << m_context->TEX0.TH);

	GSVector4 WH(tw, th, w, h);

	m_ps_sel.fst = !!PRIM->FST;

	ps_cb.WH = WH;
	ps_cb.HalfTexel = GSVector4(-0.5f, 0.5f).xxyy() / WH.zwzw();
	if (complex_wms_wmt) {
		ps_cb.MskFix = GSVector4i(m_context->CLAMP.MINU, m_context->CLAMP.MINV, m_context->CLAMP.MAXU, m_context->CLAMP.MAXV);
		ps_cb.MinMax = GSVector4(ps_cb.MskFix) / WH.xyxy();
	} else if (trilinear_manual) {
		// Reuse MinMax for mipmap parameter to avoid an extension of the UBO
		ps_cb.MinMax.x = (float)m_context->TEX1.K / 16.0f;
		ps_cb.MinMax.y = float(1 << m_context->TEX1.L);
		ps_cb.MinMax.z = float(m_lod.x); // Offset because first layer is m_lod, dunno if we can do better
		ps_cb.MinMax.w = float(m_lod.y);
	} else if (trilinear_auto) {
		tex->m_texture->GenerateMipmap();
	}

	// TC Offset Hack
	m_ps_sel.tcoffsethack = m_userhacks_tcoffset;
	ps_cb.TC_OH_TS = GSVector4(1/16.0f, 1/16.0f, m_userhacks_tcoffset_x, m_userhacks_tcoffset_y) / WH.xyxy();

	// Must be done after all coordinates math
	if (m_context->HasFixedTEX0() && !PRIM->FST) {
		m_ps_sel.invalid_tex0 = 1;
		// Use invalid size to denormalize ST coordinate
		ps_cb.WH.x = (float)(1 << m_context->stack.TEX0.TW);
		ps_cb.WH.y = (float)(1 << m_context->stack.TEX0.TH);
	}

	// Only enable clamping in CLAMP mode. REGION_CLAMP will be done manually in the shader
	m_ps_ssel.tau   = (wms != CLAMP_CLAMP);
	m_ps_ssel.tav   = (wmt != CLAMP_CLAMP);
	if (shader_emulated_sampler) {
		m_ps_ssel.biln  = 0;
		m_ps_ssel.aniso = 0;
		m_ps_ssel.triln = 0;
	} else {
		m_ps_ssel.biln  = bilinear;
		// Enable aniso only for triangles. Sprites are flat so aniso is likely useless (it would save perf for others primitives).
		m_ps_ssel.aniso = m_vt.m_primclass == GS_TRIANGLE_CLASS ? 1 : 0;
		m_ps_ssel.triln = trilinear;
		if (trilinear_manual) {
			m_ps_sel.manual_lod = 1;
		} else if (trilinear_auto) {
			m_ps_sel.automatic_lod = 1;
		}
	}

	// Setup Texture ressources
	dev->SetupSampler(m_ps_ssel);
	dev->PSSetShaderResources(tex->m_texture, tex->m_palette);
}

GSRendererOGL::PRIM_OVERLAP GSRendererOGL::PrimitiveOverlap()
{
	// Either 1 triangle or 1 line or 3 POINTs
	// It is bad for the POINTs but low probability that they overlap
	if (m_vertex.next < 4)
		return PRIM_OVERLAP_NO;

	if (m_vt.m_primclass != GS_SPRITE_CLASS)
		return PRIM_OVERLAP_UNKNOW; // maybe, maybe not

	// Check intersection of sprite primitive only
	const size_t count = m_vertex.next;
	PRIM_OVERLAP overlap = PRIM_OVERLAP_NO;
	const GSVertex* v = m_vertex.buff;

	m_drawlist.clear();
	size_t i = 0;
	while (i < count) {
		// In order to speed up comparison a bounding-box is accumulated. It removes a
		// loop so code is much faster (check game virtua fighter). Besides it allow to check
		// properly the Y order.

		// .x = min(v[i].XYZ.X, v[i+1].XYZ.X)
		// .y = min(v[i].XYZ.Y, v[i+1].XYZ.Y)
		// .z = max(v[i].XYZ.X, v[i+1].XYZ.X)
		// .w = max(v[i].XYZ.Y, v[i+1].XYZ.Y)
		GSVector4i all = GSVector4i(v[i].m[1]).upl16(GSVector4i(v[i+1].m[1])).upl16().xzyw();
		all = all.xyxy().blend(all.zwzw(), all > all.zwxy());

		size_t j = i + 2;
		while (j < count) {
			GSVector4i sprite = GSVector4i(v[j].m[1]).upl16(GSVector4i(v[j+1].m[1])).upl16().xzyw();
			sprite = sprite.xyxy().blend(sprite.zwzw(), sprite > sprite.zwxy());

			if (all.rintersect(sprite).rempty())
				all = all.runion_ordered(sprite);
			else
			{
				overlap = PRIM_OVERLAP_YES;
				break;
			}
			j += 2;
		}
		m_drawlist.push_back((j - i) >> 1); // Sprite count
		i = j;
	}

	return overlap;
}

void GSRendererOGL::SendDraw()
{
	GSDeviceOGL* dev = (GSDeviceOGL*)m_dev;

	if (!m_require_full_barrier && m_require_one_barrier) {
		// Need only a single barrier
		glTextureBarrier();
		dev->DrawIndexedPrimitive();
	} else if (!m_require_full_barrier) {
		// Don't need any barrier
		dev->DrawIndexedPrimitive();
	} else if (m_prim_overlap == PRIM_OVERLAP_NO) {
		// Need full barrier but a single barrier will be enough
		glTextureBarrier();
		dev->DrawIndexedPrimitive();
	} else if (m_vt.m_primclass == GS_SPRITE_CLASS) {
		const size_t nb_vertex = (m_gs_sel.sprite == 1) ? 2 : 6;

		for (size_t count = 0, p = 0, n = 0; n < m_drawlist.size(); p += count, ++n) {
			count = m_drawlist[n] * nb_vertex;
			glTextureBarrier();
			dev->DrawIndexedPrimitive(p, count);
		}
	} else {
		// FIXME: Investigate: a dynamic check to pack as many primitives as possibles
		// I'm nearly sure GS already have this kind of code (maybe we can adapt GSDirtyRect)
		const size_t nb_vertex = GSUtil::GetClassVertexCount(m_vt.m_primclass);

		for (size_t p = 0; p < m_index.tail; p += nb_vertex) {
			glTextureBarrier();
			dev->DrawIndexedPrimitive(p, nb_vertex);
		}
	}
}

void GSRendererOGL::ResetStates()
{
	m_require_one_barrier  = false;
	m_require_full_barrier = false;

	m_vs_sel.key = 0;
	m_gs_sel.key = 0;
	m_ps_sel.key = 0;

	m_ps_ssel.key  = 0;
	m_om_csel.key  = 0;
	m_om_dssel.key = 0;
}

void GSRendererOGL::DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex)
{
	GSTexture* hdr_rt = NULL;

	const GSVector2i& rtsize = ds ? ds->GetSize()  : rt->GetSize();
	const GSVector2& rtscale = ds ? ds->GetScale() : rt->GetScale();

	const bool DATE = m_context->TEST.DATE && m_context->FRAME.PSM != PSM_PSMCT24;
	bool DATE_GL42 = false;
	bool DATE_GL45 = false;
	bool DATE_one  = false;

	const bool ate_first_pass  = m_context->TEST.DoFirstPass();
	const bool ate_second_pass = m_context->TEST.DoSecondPass();

	ResetStates();
	vs_cb.TextureOffset = GSVector4(0.0f);

	GSDeviceOGL* dev = (GSDeviceOGL*)m_dev;

	// HLE implementation of the channel selection effect
	//
	// Warning it must be done at the begining because it will change the
	// vertex list (it will interact with PrimitiveOverlap and accurate
	// blending)
	EmulateChannelShuffle(&rt, tex);

	// Upscaling hack to avoid various line/grid issues
	MergeSprite(tex);

	// Always check if primitive overlap as it is used in plenty of effects.
	m_prim_overlap = PrimitiveOverlap();

	// Detect framebuffer read that will need special handling
	if ((GIFREG_FRAME_BLOCK(m_context->FRAME) == m_context->TEX0.TBP0) && PRIM->TME && m_sw_blending) {
		if ((m_context->FRAME.FBMSK == 0x00FFFFFF) && (m_vt.m_primclass == GS_TRIANGLE_CLASS)) {
			// This pattern is used by several games to emulate a stencil (shadow)
			// Ratchet & Clank, Jak do alpha integer multiplication (tfx) which is mostly equivalent to +1/-1
			// Tri-Ace (Star Ocean 3/RadiataStories/VP2) uses a palette to handle the +1/-1
			m_ps_sel.tex_is_fb = 1;
			m_require_full_barrier = true;
		} else if (m_prim_overlap != PRIM_OVERLAP_NO) {
			// Note: It is fine if the texture fits in a single GS page. First access will cache
			// the page in the GS texture buffer.
			// Source and Target are the same!
		}
	}

	EmulateTextureShuffleAndFbmask();

	// DATE: selection of the algorithm. Must be done before blending because GL42 is not compatible with blending
	if (DATE) {
		if (m_prim_overlap == PRIM_OVERLAP_NO || m_texture_shuffle) {
			// It is way too complex to emulate texture shuffle with DATE. So just use
			// the slow but accurate algo
			m_require_full_barrier = true;
			DATE_GL45 = true;
		} else if (m_om_csel.wa && !m_context->TEST.ATE) {
			// Performance note: check alpha range with GetAlphaMinMax()
			// Note: all my dump are already above 120fps, but it seems to reduce GPU load
			// with big upscaling
			GetAlphaMinMax();
			if (m_context->TEST.DATM && m_vt.m_alpha.max < 128) {
				// Only first pixel (write 0) will pass (alpha is 1)
				DATE_one = true;
			} else if (!m_context->TEST.DATM && m_vt.m_alpha.min >= 128) {
				// Only first pixel (write 1) will pass (alpha is 0)
				DATE_one = true;
			} else if ((m_vt.m_primclass == GS_SPRITE_CLASS && m_drawlist.size() < 50) || (m_index.tail < 100)) {
				// texture barrier will split the draw call into n draw call. It is very efficient for
				// few primitive draws. Otherwise it sucks.
				m_require_full_barrier = true;
				DATE_GL45 = true;
			} else {
				switch (m_accurate_date) {
					case ACC_DATE_FULL:
						if (GLLoader::found_GL_ARB_shader_image_load_store && GLLoader::found_GL_ARB_clear_texture) {
							DATE_GL42 = true;
						} else {
							m_require_full_barrier = true;
							DATE_GL45 = true;
						}
						break;
					case ACC_DATE_FAST:
						DATE_one = true;
						break;
					case ACC_DATE_NONE:
					default:
						break;
				}
			}
		} else if (!m_om_csel.wa && !m_context->TEST.ATE) {
			// TODO: is it legal ? Likely but it need to be tested carefully
			// DATE_GL45 = true;
			// m_require_one_barrier = true; << replace it with a cheap barrier

		}
	}

	// Blend

	if (!IsOpaque() && rt) {
		EmulateBlending(DATE_GL42, DATE_GL45);
	} else {
		dev->OMSetBlendState(); // No blending please
	}

	if (m_ps_sel.dfmt == 1) {
		// Disable writing of the alpha channel
		m_om_csel.wa = 0;
	}

	// DATE setup, no DATE_GL45 please

	if (DATE && !DATE_GL45) {
		const GSVector4i dRect = ComputeBoundingBox(rtscale, rtsize);

		// Reduce the quantity of clean function
		glScissor( dRect.x, dRect.y, dRect.width(), dRect.height() );
		GLState::scissor = dRect;

		// Must be done here to avoid any GL state pertubation (clear function...)
		// Create an r32ui image that will containt primitive ID
		if (DATE_GL42) {
			dev->InitPrimDateTexture(rt, dRect);
		} else if (DATE_one) {
			dev->ClearStencil(ds, 1);
		} else {
			const GSVector4 src = GSVector4(dRect) / GSVector4(rtsize.x, rtsize.y).xyxy();
			const GSVector4 dst = src * 2.0f - 1.0f;

			GSVertexPT1 vertices[] =
			{
				{GSVector4(dst.x, dst.y, 0.0f, 0.0f), GSVector2(src.x, src.y)},
				{GSVector4(dst.z, dst.y, 0.0f, 0.0f), GSVector2(src.z, src.y)},
				{GSVector4(dst.x, dst.w, 0.0f, 0.0f), GSVector2(src.x, src.w)},
				{GSVector4(dst.z, dst.w, 0.0f, 0.0f), GSVector2(src.z, src.w)},
			};

			dev->SetupDATE(rt, ds, vertices, m_context->TEST.DATM);
		}
	}

	// om

	EmulateZbuffer(); // will update VS depth mask

	// vs

	// FIXME D3D11 and GL support half pixel center. Code could be easier!!!
	const float sx = 2.0f * rtscale.x / (rtsize.x << 4);
	const float sy = 2.0f * rtscale.y / (rtsize.y << 4);
	const float ox = (float)(int)m_context->XYOFFSET.OFX;
	const float oy = (float)(int)m_context->XYOFFSET.OFY;
	float ox2 = -1.0f / rtsize.x;
	float oy2 = -1.0f / rtsize.y;

	//This hack subtracts around half a pixel from OFX and OFY.
	//
	//The resulting shifted output aligns better with common blending / corona / blurring effects,
	//but introduces a few bad pixels on the edges.

	if (rt && rt->OffsetHack_modxy > 1.0f)
	{
		ox2 *= rt->OffsetHack_modxy;
		oy2 *= rt->OffsetHack_modxy;
	}

	// Note: DX does y *= -1.0
	vs_cb.Vertex_Scale_Offset = GSVector4(sx, sy, ox * sx + ox2 + 1, oy * sy + oy2 + 1);
	// END of FIXME

	// GS_SPRITE_CLASS are already flat (either by CPU or the GS)
	m_ps_sel.iip = (m_vt.m_primclass == GS_SPRITE_CLASS) ? 1 : PRIM->IIP;

	if (DATE_GL45) {
		m_ps_sel.date = 5 + m_context->TEST.DATM;
	} else if (DATE_one) {
		m_require_one_barrier = true;
		m_ps_sel.date       = 5 + m_context->TEST.DATM;
		m_om_dssel.date     = 1;
		m_om_dssel.date_one = 1;
	} else if (DATE) {
		if (DATE_GL42)
			m_ps_sel.date = 1 + m_context->TEST.DATM;
		else
			m_om_dssel.date = 1;
	}

	m_ps_sel.fba = m_context->FBA.FBA;
	m_ps_sel.dither = m_dithering > 0 && m_ps_sel.dfmt == 2 && m_env.DTHE.DTHE;

	if (m_ps_sel.dither)
	{
		m_ps_sel.dither = m_dithering;
		ps_cb.DitherMatrix[0] = GSVector4(m_env.DIMX.DM00, m_env.DIMX.DM01, m_env.DIMX.DM02, m_env.DIMX.DM03);
		ps_cb.DitherMatrix[1] = GSVector4(m_env.DIMX.DM10, m_env.DIMX.DM11, m_env.DIMX.DM12, m_env.DIMX.DM13);
		ps_cb.DitherMatrix[2] = GSVector4(m_env.DIMX.DM20, m_env.DIMX.DM21, m_env.DIMX.DM22, m_env.DIMX.DM23);
		ps_cb.DitherMatrix[3] = GSVector4(m_env.DIMX.DM30, m_env.DIMX.DM31, m_env.DIMX.DM32, m_env.DIMX.DM33);
	}

	if (PRIM->FGE)
	{
		m_ps_sel.fog = 1;

		const GSVector4 fc = GSVector4::rgba32(m_env.FOGCOL.U32[0]);
#if _M_SSE >= 0x401
		// Blend AREF to avoid to load a random value for alpha (dirty cache)
		ps_cb.FogColor_AREF = fc.blend32<8>(ps_cb.FogColor_AREF);
#else
		ps_cb.FogColor_AREF = fc;
#endif
	}

	// Warning must be done after EmulateZbuffer
	// Depth test is always true so it can be executed in 2 passes (no order required) unlike color.
	// The idea is to compute first the color which is independent of the alpha test. And then do a 2nd
	// pass to handle the depth based on the alpha test.
	bool ate_RGBA_then_Z = false;
	bool ate_RGB_then_ZA = false;
	if (ate_first_pass & ate_second_pass) {
		bool commutative_depth = (m_om_dssel.ztst == ZTST_GEQUAL && m_vt.m_eq.z) || (m_om_dssel.ztst == ZTST_ALWAYS);
		bool commutative_alpha = (m_context->ALPHA.C != 1); // when either Alpha Src or a constant

		ate_RGBA_then_Z = (m_context->TEST.AFAIL == AFAIL_FB_ONLY) & commutative_depth;
		ate_RGB_then_ZA = (m_context->TEST.AFAIL == AFAIL_RGB_ONLY) & commutative_depth & commutative_alpha;
	}

	if (ate_RGBA_then_Z) {
		// Render all color but don't update depth
		// ATE is disabled here
		m_om_dssel.zwe = false;
	} else if (ate_RGB_then_ZA) {
		// Render RGB color but don't update depth/alpha
		// ATE is disabled here
		m_om_dssel.zwe = false;
		m_om_csel.wa = false;
	} else {
		EmulateAtst(1, tex);
	}

	if (tex) {
		EmulateTextureSampler(tex);
	} else {
		m_ps_sel.tfx = 4;
	}

	// Always bind the RT. This way special effect can use it.
	dev->PSSetShaderResource(3, rt);

	if (m_game.title == CRC::ICO) {
		const GSVertex* v = &m_vertex.buff[0];
		const GSVideoMode mode = GetVideoMode();
		if (tex && m_vt.m_primclass == GS_SPRITE_CLASS && m_vertex.next == 2 && PRIM->ABE && // Blend texture
				((v[1].U == 8200 && v[1].V == 7176 && mode == GSVideoMode::NTSC) || // at display resolution 512x448
				(v[1].U == 8200 && v[1].V == 8200 && mode == GSVideoMode::PAL)) && // at display resolution 512x512
				tex->m_TEX0.PSM == PSM_PSMT8H) {  // i.e. read the alpha channel of a 32 bits texture
			// Note potentially we can limit to TBP0:0x2800

			// Depth buffer was moved so GS will invalidate it which means a
			// downscale. ICO uses the MSB depth bits as the texture alpha
			// channel.  However this depth of field effect requires
			// texel:pixel mapping accuracy.
			//
			// Use an HLE shader to sample depth directly as the alpha channel
			// ICO sample depth as alpha
			m_require_full_barrier = true;
			// Extract the depth as palette index
			m_ps_sel.depth_fmt = 1;
			m_ps_sel.channel = ChannelFetch_BLUE;
			dev->PSSetShaderResource(4, ds);

			// We need the palette to convert the depth to the correct alpha value.
			if (!tex->m_palette) {
				const u16 pal = GSLocalMemory::m_psm[tex->m_TEX0.PSM].pal;
				m_tc->AttachPaletteToSource(tex, pal, true);
				dev->PSSetShaderResource(1, tex->m_palette);
			}
		}
	}

	// rs
	const GSVector4& hacked_scissor = m_channel_shuffle ? GSVector4(0, 0, 1024, 1024) : m_context->scissor.in;
	const GSVector4i scissor = GSVector4i(GSVector4(rtscale).xyxy() * hacked_scissor).rintersect(GSVector4i(rtsize).zwxy());

	SetupIA(sx, sy);

	dev->OMSetColorMaskState(m_om_csel);
	dev->SetupOM(m_om_dssel);

	dev->SetupCB(&vs_cb, &ps_cb);

	dev->SetupPipeline(m_vs_sel, m_gs_sel, m_ps_sel);

	const GSVector4i commitRect = ComputeBoundingBox(rtscale, rtsize);

	if (DATE_GL42)
	{
		// It could be good idea to use stencil in the same time.
		// Early stencil test will reduce the number of atomic-load operation

		// Create an r32i image that will contain primitive ID
		// Note: do it at the beginning because the clean will dirty the FBO state
		//dev->InitPrimDateTexture(rtsize.x, rtsize.y);

		// I don't know how much is it legal to mount rt as Texture/RT. No write is done.
		// In doubt let's detach RT.
		dev->OMSetRenderTargets(NULL, ds, &scissor);

		// Don't write anything on the color buffer
		// Neither in the depth buffer
		glDepthMask(false);
		// Compute primitiveID max that pass the date test (Draw without barrier)
		dev->DrawIndexedPrimitive();

		// Ask PS to discard shader above the primitiveID max
		glDepthMask(GLState::depth_mask);

		m_ps_sel.date = 3;
		dev->SetupPipeline(m_vs_sel, m_gs_sel, m_ps_sel);

		// Be sure that first pass is finished !
		dev->Barrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	if (m_ps_sel.hdr) {
		hdr_rt = dev->CreateRenderTarget(rtsize.x, rtsize.y, GL_RGBA32F);

		dev->CopyRectConv(rt, hdr_rt, ComputeBoundingBox(rtscale, rtsize), false);

		dev->OMSetRenderTargets(hdr_rt, ds, &scissor);
	} else {
		dev->OMSetRenderTargets(rt, ds, &scissor);
	}

	if (ate_first_pass)
	{
		SendDraw();
	}

	if (ate_second_pass)
	{
		if (ate_RGBA_then_Z | ate_RGB_then_ZA) {
			// Enable ATE as first pass to update the depth
			// of pixels that passed the alpha test
			EmulateAtst(1, tex);
		} else {
			// second pass will process the pixels that failed
			// the alpha test
			EmulateAtst(2, tex);
		}

		// Potentially AREF was updated (hope perf impact will be limited)
		dev->SetupCB(&vs_cb, &ps_cb);

		dev->SetupPipeline(m_vs_sel, m_gs_sel, m_ps_sel);

		bool z = m_om_dssel.zwe;
		bool r = m_om_csel.wr;
		bool g = m_om_csel.wg;
		bool b = m_om_csel.wb;
		bool a = m_om_csel.wa;

		switch(m_context->TEST.AFAIL)
		{
			case AFAIL_KEEP: z = r = g = b = a = false; break; // none
			case AFAIL_FB_ONLY: z = false; break; // rgba
			case AFAIL_ZB_ONLY: r = g = b = a = false; break; // z
			case AFAIL_RGB_ONLY: z = a = false; break; // rgb
			default: break;
		}

		// Depth test should be disabled when depth writes are masked and similarly, Alpha test must be disabled
		// when writes to all of the alpha bits in the Framebuffer are masked.
		if (ate_RGBA_then_Z) {
			z = !m_context->ZBUF.ZMSK;
			r = g = b = a = false;
		} else if (ate_RGB_then_ZA) {
			z = !m_context->ZBUF.ZMSK;
			a = (m_context->FRAME.FBMSK & 0xFF000000) != 0xFF000000;
			r = g = b = false;
		}

		if (z || r || g || b || a)
		{
			m_om_dssel.zwe = z;
			m_om_csel.wr = r;
			m_om_csel.wg = g;
			m_om_csel.wb = b;
			m_om_csel.wa = a;

			dev->OMSetColorMaskState(m_om_csel);
			dev->SetupOM(m_om_dssel);

			SendDraw();
		}
	}

	if (DATE_GL42) {
		dev->RecycleDateTexture();
	}

	dev->EndScene();

	// Warning: EndScene must be called before StretchRect otherwise
	// vertices will be overwritten. Trust me you don't want to do that.
	if (hdr_rt) {
		const GSVector4 dRect(ComputeBoundingBox(rtscale, rtsize));
		const GSVector4 sRect = dRect / GSVector4(rtsize.x, rtsize.y).xyxy();
		dev->StretchRect(hdr_rt, sRect, rt, dRect, ShaderConvert_MOD_256, false);

		dev->Recycle(hdr_rt);
	}
}

bool GSRendererOGL::IsDummyTexture() const
{
	// Texture is actually the frame buffer. Stencil emulation to compute shadow (Jak series/tri-ace game)
	// Will hit the "m_ps_sel.tex_is_fb = 1" path in the draw
	return (GIFREG_FRAME_BLOCK(m_context->FRAME) == m_context->TEX0.TBP0) && PRIM->TME && m_sw_blending && m_vt.m_primclass == GS_TRIANGLE_CLASS && (m_context->FRAME.FBMSK == 0x00FFFFFF);
}
