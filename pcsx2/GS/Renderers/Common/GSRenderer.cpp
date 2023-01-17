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

#include "GSRenderer.h"
#include "options_tools.h"

GSRenderer::GSRenderer()
	: m_texture_shuffle(false)
	, m_real_size(0,0)
	, m_dev(NULL)
{
	const unsigned int s_interlace_nb = 8;
	m_interlace   = option_value(INT_PCSX2_OPT_DEINTERLACING_MODE, KeyOptionInt::return_type) % s_interlace_nb;
	m_fxaa        = option_value(INT_PCSX2_OPT_FXAA, KeyOptionInt::return_type);
	m_dithering   = option_value(INT_PCSX2_OPT_DITHERING, KeyOptionInt::return_type); // 0 off, 1 auto, 2 auto no scale
}

GSRenderer::~GSRenderer()
{
	delete m_dev;
}

void GSRenderer::ResetDevice()
{
    if(m_dev) m_dev->Reset(1, 1);
}

bool GSRenderer::Merge(int field)
{
	bool en[2];

	GSVector4i fr[2];
	GSVector4i dr[2];

	GSVector2i display_baseline = { INT_MAX, INT_MAX };
	GSVector2i frame_baseline = { INT_MAX, INT_MAX };

        en[0] = IsEnabled(0);
        en[1] = IsEnabled(1);

	if(!en[0] && !en[1])
		return false;

	for(int i = 0; i < 2; i++)
	{
		if(en[i])
		{
			fr[i] = GetFrameRect(i);
			dr[i] = GetDisplayRect(i);

			display_baseline.x = std::min(dr[i].left, display_baseline.x);
			display_baseline.y = std::min(dr[i].top, display_baseline.y);
			frame_baseline.x = std::min(fr[i].left, frame_baseline.x);
			frame_baseline.y = std::min(fr[i].top, frame_baseline.y);
		}
	}


	// try to avoid fullscreen blur, could be nice on tv but on a monitor it's like double vision, hurts my eyes (persona 4, guitar hero)
	//
	// NOTE: probably the technique explained in graphtip.pdf (Antialiasing by Supersampling / 4. Reading Odd/Even Scan Lines Separately with the PCRTC then Blending)

	bool samesrc =
		en[0] && en[1] &&
		m_regs->DISP[0].DISPFB.FBP == m_regs->DISP[1].DISPFB.FBP &&
		m_regs->DISP[0].DISPFB.FBW == m_regs->DISP[1].DISPFB.FBW &&
		GSUtil::HasCompatibleBits(m_regs->DISP[0].DISPFB.PSM, m_regs->DISP[1].DISPFB.PSM);

	if(samesrc /*&& m_regs->PMODE.SLBG == 0 && m_regs->PMODE.MMOD == 1 && m_regs->PMODE.ALP == 0x80*/)
	{
		// Persona 4:
		//
		// fr[0] = 0 0 640 448
		// fr[1] = 0 1 640 448
		// dr[0] = 159 50 779 498
		// dr[1] = 159 50 779 497
		//
		// second image shifted up by 1 pixel and blended over itself
		//
		// God of War:
		//
		// fr[0] = 0 1 512 448
		// fr[1] = 0 0 512 448
		// dr[0] = 127 50 639 497
		// dr[1] = 127 50 639 498
		//
		// same just the first image shifted
		//
		// These kinds of cases are now fixed by the more generic frame_diff code below, as the code here was too specific and has become obsolete.
		// NOTE: Persona 4 and God Of War are not rare exceptions, many games have the same(or very similar) offsets.

		int topDiff = fr[0].top - fr[1].top;
		if (dr[0].eq(dr[1]) && (fr[0].eq(fr[1] + GSVector4i(0, topDiff, 0, topDiff)) || fr[1].eq(fr[0] + GSVector4i(0, topDiff, 0, topDiff))))
		{
			// Dragon Quest 5:
			//
			// fr[0] = 0 1 512 445
			// fr[1] = 0 0 512 444
			// dr[0] = 127 50 639 494
			// dr[1] = 127 50 639 494

			int top = std::min(fr[0].top, fr[1].top);
			int bottom = std::min(fr[0].bottom, fr[1].bottom);

			fr[0].top = fr[1].top = top;
			fr[0].bottom = fr[1].bottom = bottom;
		}
	}

	GSVector2i fs(0, 0);
	GSVector2i ds(0, 0);

	GSTexture* tex[3] = {NULL, NULL, NULL};
	int y_offset[3]   = {0, 0, 0};

	bool feedback_merge = m_regs->EXTWRITE.WRITE == 1;

	if(samesrc && fr[0].bottom == fr[1].bottom && !feedback_merge)
	{
		tex[0]      = GetOutput(0, y_offset[0]);
		tex[1]      = tex[0]; // saves one texture fetch
		y_offset[1] = y_offset[0];
	}
	else
	{
		if(en[0]) tex[0] = GetOutput(0, y_offset[0]);
		if(en[1]) tex[1] = GetOutput(1, y_offset[1]);
		if(feedback_merge) tex[2] = GetFeedbackOutput();
	}

	GSVector4 src[2];
	GSVector4 src_hw[2];
	GSVector4 dst[2];

	for(int i = 0; i < 2; i++)
	{
		if(!en[i] || !tex[i]) continue;

		GSVector4i r = fr[i];
		GSVector4 scale = GSVector4(tex[i]->GetScale()).xyxy();

		src[i] = GSVector4(r) * scale / GSVector4(tex[i]->GetSize()).xyxy();
		src_hw[i] = (GSVector4(r) + GSVector4 (0, y_offset[i], 0, y_offset[i])) * scale / GSVector4(tex[i]->GetSize()).xyxy();

		GSVector2 off(0);
		GSVector2i display_diff(dr[i].left - display_baseline.x, dr[i].top - display_baseline.y);
		GSVector2i frame_diff(fr[i].left - frame_baseline.x, fr[i].top - frame_baseline.y);

		// Time Crisis 2/3 uses two side by side images when in split screen mode.
		// Though ignore cases where baseline and display rectangle offsets only differ by 1 pixel, causes blurring and wrong resolution output on FFXII
		if(display_diff.x > 2)
			off.x = tex[i]->GetScale().x * display_diff.x;
		// If the DX offset is too small then consider the status of frame memory offsets, prevents blurring on Tenchu: Fatal Shadows, Worms 3D
		else if(display_diff.x != frame_diff.x)
			off.x = tex[i]->GetScale().x * frame_diff.x;

		if(display_diff.y >= 4) // Shouldn't this be >= 2?
		{
			off.y = tex[i]->GetScale().y * display_diff.y;

			if(m_regs->SMODE2.INT && m_regs->SMODE2.FFMD)
				off.y /= 2;
		}
		else if(display_diff.y != frame_diff.y)
			off.y = tex[i]->GetScale().y * frame_diff.y;

		dst[i] = GSVector4(off).xyxy() + scale * GSVector4(r.rsize());

		fs.x = std::max(fs.x, (int)(dst[i].z + 0.5f));
		fs.y = std::max(fs.y, (int)(dst[i].w + 0.5f));
	}

	ds = fs;

	// When interlace(FRAME) mode, the rect is half height, so it needs to be stretched.
	if(m_regs->SMODE2.INT && m_regs->SMODE2.FFMD)
		ds.y *= 2;

	m_real_size = ds;

	bool slbg = m_regs->PMODE.SLBG;

	if(tex[0] || tex[1])
	{
		// the two outputs are identical, skip drawing one of them (the one that is alpha blended)
		if(tex[0] == tex[1] && !slbg && (src[0] == src[1] & dst[0] == dst[1]).alltrue())
			tex[0] = NULL;

		GSVector4 c = GSVector4((int)m_regs->BGCOLOR.R, (int)m_regs->BGCOLOR.G, (int)m_regs->BGCOLOR.B, (int)m_regs->PMODE.ALP) / 255;

		m_dev->Merge(tex, src_hw, dst, fs, m_regs->PMODE, m_regs->EXTBUF, c);

		if(isReallyInterlaced() && m_interlace > 0)
		{
			if(m_interlace == 7 && m_regs->SMODE2.FFMD) // Auto interlace enabled / Odd frame interlace setting
			{
				int field2 = 0;
				int mode = 2;
				m_dev->Interlace(ds, field ^ field2, mode, tex[1] ? tex[1]->GetScale().y : tex[0]->GetScale().y);
			}
			else
			{
				int field2 = 1 - ((m_interlace - 1) & 1);
				int mode = (m_interlace - 1) >> 1;
				m_dev->Interlace(ds, field ^ field2, mode, tex[1] ? tex[1]->GetScale().y : tex[0]->GetScale().y);
			}
		}

		if(m_fxaa)
			m_dev->FXAA();
	}

	return true;
}

GSVector2i GSRenderer::GetInternalResolution()
{
	return m_real_size;
}

void GSRenderer::VSync(int field)
{
	Flush();

	if(!Merge(field ? 1 : 0))
		return;

	m_dev->AgePool();

	// present
	m_dev->Present(GSClientRect(), 0);
}

void GSRenderer::PurgePool()
{
	m_dev->PurgePool();
}

void GSRenderer::UpdateRendererOptions()
{
}
