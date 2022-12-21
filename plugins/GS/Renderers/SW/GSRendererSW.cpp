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

#include "GSRendererSW.h"

GSVector4 GSRendererSW::m_pos_scale;
#if _M_SSE >= 0x501
GSVector8 GSRendererSW::m_pos_scale2;
#endif

void GSRendererSW::InitVectors()
{
	m_pos_scale = GSVector4(1.0f / 16, 1.0f / 16, 1.0f, 128.0f);

#if _M_SSE >= 0x501
	m_pos_scale2 = GSVector8(1.0f / 16, 1.0f / 16, 1.0f, 128.0f, 1.0f / 16, 1.0f / 16, 1.0f, 128.0f);
#endif
}

GSRendererSW::GSRendererSW(int threads)
	: m_fzb(NULL)
{
	m_nativeres = true; // ignore ini, sw is always native

	m_tc = new GSTextureCacheSW(this);

	memset(m_texture, 0, sizeof(m_texture));

	m_rl = GSRasterizerList::Create<GSDrawScanline>(threads);

	m_output = (u8*)_aligned_malloc(1024 * 1024 * sizeof(u32), 32);

	for (u32 i = 0; i < ARRAY_SIZE(m_fzb_pages); i++)
		m_fzb_pages[i] = 0;
	for (u32 i = 0; i < ARRAY_SIZE(m_tex_pages); i++)
		m_tex_pages[i] = 0;

	#define InitCVB2(P, Q) \
		m_cvb[P][0][0][Q] = &GSRendererSW::ConvertVertexBuffer<P, 0, 0, Q>; \
		m_cvb[P][0][1][Q] = &GSRendererSW::ConvertVertexBuffer<P, 0, 1, Q>; \
		m_cvb[P][1][0][Q] = &GSRendererSW::ConvertVertexBuffer<P, 1, 0, Q>; \
		m_cvb[P][1][1][Q] = &GSRendererSW::ConvertVertexBuffer<P, 1, 1, Q>;

	#define InitCVB(P) \
		InitCVB2(P, 0) \
		InitCVB2(P, 1)

	InitCVB(GS_POINT_CLASS);
	InitCVB(GS_LINE_CLASS);
	InitCVB(GS_TRIANGLE_CLASS);
	InitCVB(GS_SPRITE_CLASS);

	// Reset handler with the auto flush hack enabled on the SW renderer.
	// Some games run better without the hack so rely on ini/gui option.
	if (theApp.GetConfigB("autoflush_sw")) {
		m_userhacks_auto_flush = true;
		ResetHandlers();
	}
}

GSRendererSW::~GSRendererSW()
{
	delete m_tc;

	for(size_t i = 0; i < ARRAY_SIZE(m_texture); i++)
		delete m_texture[i];

	delete m_rl;

	_aligned_free(m_output);
}

void GSRendererSW::Reset()
{
	Sync(-1);

	m_tc->RemoveAll();

	GSRenderer::Reset();
}

void GSRendererSW::VSync(int field)
{
	Sync(0); // IncAge might delete a cached texture in use
	GSRenderer::VSync(field);
	m_tc->IncAge();
}

void GSRendererSW::ResetDevice()
{
	for(size_t i = 0; i < ARRAY_SIZE(m_texture); i++)
	{
		delete m_texture[i];

		m_texture[i] = NULL;
	}
}

GSTexture* GSRendererSW::GetOutput(int i, int& y_offset)
{
	Sync(1);

	const GSRegDISPFB& DISPFB = m_regs->DISP[i].DISPFB;

	int w = DISPFB.FBW * 64;
	int h = GetFramebufferHeight();

	// TODO: round up bottom

	if(m_dev->ResizeTexture(&m_texture[i], w, h))
	{
		static int pitch = 1024 * 4;

		GSVector4i r(0, 0, w, h);

		const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[DISPFB.PSM];

		(m_mem.*psm.rtx)(m_mem.GetOffset(DISPFB.Block(), DISPFB.FBW, DISPFB.PSM), r.ralign<Align_Outside>(psm.bs), m_output, pitch, m_env.TEXA);

		m_texture[i]->Update(r, m_output, pitch);
	}

	return m_texture[i];
}

GSTexture* GSRendererSW::GetFeedbackOutput()
{
	int dummy;

	// It is enough to emulate Xenosaga cutscene. (or any game that will do a basic loopback)
	for (int i = 0; i < 2; i++) {
		if (m_regs->EXTBUF.EXBP == m_regs->DISP[i].DISPFB.Block())
			return GetOutput(i, dummy);
	}

	return nullptr;
}


template<u32 primclass, u32 tme, u32 fst, u32 q_div>
void GSRendererSW::ConvertVertexBuffer(GSVertexSW* RESTRICT dst, const GSVertex* RESTRICT src, size_t count)
{
	GSVector4i off = (GSVector4i)m_context->XYOFFSET;
	GSVector4 tsize = GSVector4(0x10000 << m_context->TEX0.TW, 0x10000 << m_context->TEX0.TH, 1, 0);

	#if _M_SSE >= 0x401

	GSVector4i z_max = GSVector4i::xffffffff().srl32(GSLocalMemory::m_psm[m_context->ZBUF.PSM].fmt * 8);

	#else

	u32 z_max = 0xffffffff >> (GSLocalMemory::m_psm[m_context->ZBUF.PSM].fmt * 8);

	#endif

	for(int i = (int)m_vertex.next; i > 0; i--, src++, dst++)
	{
		GSVector4 stcq = GSVector4::load<true>(&src->m[0]); // s t rgba q

		#if _M_SSE >= 0x401

		GSVector4i xyzuvf(src->m[1]);

		GSVector4i xy = xyzuvf.upl16() - off;
		GSVector4i zf = xyzuvf.ywww().min_u32(GSVector4i::xffffff00());

		#else

		u32 z = src->XYZ.Z;

		GSVector4i xy = GSVector4i::load((int)src->XYZ.U32[0]).upl16() - off;
		GSVector4i zf = GSVector4i((int)std::min<u32>(z, 0xffffff00), src->FOG); // NOTE: larger values of z may roll over to 0 when converting back to u32 later

		#endif

		dst->p = GSVector4(xy).xyxy(GSVector4(zf) + (GSVector4::m_x4f800000 & GSVector4::cast(zf.sra32(31)))) * m_pos_scale;
		dst->c = GSVector4(GSVector4i::cast(stcq).zzzz().u8to32() << 7);

		GSVector4 t = GSVector4::zero();

		if(tme)
		{
			if(fst)
			{
				#if _M_SSE >= 0x401

				t = GSVector4(xyzuvf.uph16() << (16 - 4));

				#else

				t = GSVector4(GSVector4i::load(src->UV).upl16() << (16 - 4));

				#endif
			}
			else if(q_div)
			{
				// Division is required if number are huge (Pro Soccer Club)
				if(primclass == GS_SPRITE_CLASS && (i & 1) == 0)
				{
					// q(n) isn't valid, you need to take q(n+1)
					const GSVertex* next = src + 1;
					GSVector4 stcq1 = GSVector4::load<true>(&next->m[0]); // s t rgba q
					t = (stcq / stcq1.wwww()) * tsize;
				}
				else
				{
					t = (stcq / stcq.wwww()) * tsize;
				}
			}
			else
			{
				t = stcq.xyww() * tsize;
			}
		}

		if(primclass == GS_SPRITE_CLASS)
		{
			#if _M_SSE >= 0x401

			xyzuvf = xyzuvf.min_u32(z_max);
			t = t.insert32<1, 3>(GSVector4::cast(xyzuvf));

			#else

			z = std::min(z, z_max);
			t = t.insert32<0, 3>(GSVector4::cast(GSVector4i::load(z)));

			#endif
		}

		dst->t = t;
	}
}

void GSRendererSW::Draw()
{
	const GSDrawingContext* context = m_context;

	SharedData* sd = new SharedData(this);

	std::shared_ptr<GSRasterizerData> data(sd);

	sd->primclass = m_vt.m_primclass;
	sd->buff = (u8*)_aligned_malloc(sizeof(GSVertexSW) * ((m_vertex.next + 1) & ~1) + sizeof(u32) * m_index.tail, 64);
	sd->vertex = (GSVertexSW*)sd->buff;
	sd->vertex_count = m_vertex.next;
	sd->index = (u32*)(sd->buff + sizeof(GSVertexSW) * ((m_vertex.next + 1) & ~1));
	sd->index_count = m_index.tail;

	// skip per pixel division if q is constant.
	// Optimize the division by 1 with a nop. It also means that GS_SPRITE_CLASS must be processed when !m_vt.m_eq.q.
	// If you have both GS_SPRITE_CLASS && m_vt.m_eq.q, it will depends on the first part of the 'OR'
	bool is_mipmap_active = m_mipmap && IsMipMapDraw();
	u32 q_div = !is_mipmap_active && ((m_vt.m_eq.q && m_vt.m_min.t.z != 1.0f) || (!m_vt.m_eq.q && m_vt.m_primclass == GS_SPRITE_CLASS));

	(this->*m_cvb[m_vt.m_primclass][PRIM->TME][PRIM->FST][q_div])(sd->vertex, m_vertex.buff, m_vertex.next);

	memcpy(sd->index, m_index.buff, sizeof(u32) * m_index.tail);

	GSVector4i scissor = GSVector4i(context->scissor.in);
	GSVector4i bbox = GSVector4i(m_vt.m_min.p.floor().xyxy(m_vt.m_max.p.ceil()));

	// points and lines may have zero area bbox (single line: 0, 0 - 256, 0)

	if(m_vt.m_primclass == GS_POINT_CLASS || m_vt.m_primclass == GS_LINE_CLASS)
	{
		if(bbox.x == bbox.z) bbox.z++;
		if(bbox.y == bbox.w) bbox.w++;
	}

	GSVector4i r = bbox.rintersect(scissor);

	scissor.z = std::min<int>(scissor.z, (int)context->FRAME.FBW * 64); // TODO: find a game that overflows and check which one is the right behaviour
	
	sd->scissor = scissor;
	sd->bbox    = bbox;

	if(!GetScanlineGlobalData(sd))
		return;

	//

	// GSScanlineGlobalData& gd = sd->global;

	u32* fb_pages = NULL;
	u32* zb_pages = NULL;

	if(sd->global.sel.fb)
	{
		fb_pages = m_context->offset.fb->GetPages(r);
	}

	if(sd->global.sel.zb)
	{
		zb_pages = m_context->offset.zb->GetPages(r);
	}

	// check if there is an overlap between this and previous targets

	if(CheckTargetPages(fb_pages, zb_pages, r))
	{
		sd->m_syncpoint = SharedData::SyncTarget;
	}

	// check if the texture is not part of a target currently in use

	if(CheckSourcePages(sd))
	{
		sd->m_syncpoint = SharedData::SyncSource;
	}

	// addref source and target pages

	sd->UsePages(fb_pages, m_context->offset.fb->psm, zb_pages, m_context->offset.zb->psm);

	//

	{
		Queue(data);
	}
}

void GSRendererSW::Queue(std::shared_ptr<GSRasterizerData>& item)
{
	SharedData* sd = (SharedData*)item.get();

	if(sd->m_syncpoint == SharedData::SyncSource) 
	{
		Sync(4);
	}

	// update previously invalidated parts

	sd->UpdateSource();

	if(sd->m_syncpoint == SharedData::SyncTarget)
	{
		Sync(5);
	}

	m_rl->Queue(item);

	// invalidate new parts rendered onto

	if(sd->global.sel.fwrite)
	{
		m_tc->InvalidatePages(sd->m_fb_pages, sd->m_fpsm);

		m_mem.m_clut.Invalidate(GIFREG_FRAME_BLOCK(m_context->FRAME));
	}

	if(sd->global.sel.zwrite)
	{
		m_tc->InvalidatePages(sd->m_zb_pages, sd->m_zpsm);
	}
}

void GSRendererSW::Sync(int reason)
{
	m_rl->Sync();
}

void GSRendererSW::InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	GSOffset* off = m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM);

	off->GetPages(r, m_tmp_pages);

	// check if the changing pages either used as a texture or a target

	if(!m_rl->IsSynced())
	{
		for(u32* RESTRICT p = m_tmp_pages; *p != GSOffset::EOP; p++)
		{
			if(m_fzb_pages[*p] | m_tex_pages[*p])
			{
				Sync(6);

				break;
			}
		}
	}

	m_tc->InvalidatePages(m_tmp_pages, off->psm); // if texture update runs on a thread and Sync(5) happens then this must come later
}

void GSRendererSW::InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut)
{
	if(!m_rl->IsSynced())
	{
		GSOffset* off = m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM);

		off->GetPages(r, m_tmp_pages);

		for(u32* RESTRICT p = m_tmp_pages; *p != GSOffset::EOP; p++)
		{
			if(m_fzb_pages[*p])
			{
				Sync(7);

				break;
			}
		}
	}
}

void GSRendererSW::UsePages(const u32* pages, const int type)
{
	for(const u32* p = pages; *p != GSOffset::EOP; p++) {
		switch (type) {
			case 0:
				ASSERT((m_fzb_pages[*p] & 0xFFFF) < USHRT_MAX);
				m_fzb_pages[*p] += 1;
				break;
			case 1:
				ASSERT((m_fzb_pages[*p] >> 16) < USHRT_MAX);
				m_fzb_pages[*p] += 0x10000;
				break;
			case 2:
				ASSERT(m_tex_pages[*p] < USHRT_MAX);
				m_tex_pages[*p] += 1;
				break;
			default:break;
		}
	}
}

void GSRendererSW::ReleasePages(const u32* pages, const int type)
{
	for(const u32* p = pages; *p != GSOffset::EOP; p++) {
		switch (type) {
			case 0:
				ASSERT((m_fzb_pages[*p] & 0xFFFF) > 0);
				m_fzb_pages[*p] -= 1;
				break;
			case 1:
				ASSERT((m_fzb_pages[*p] >> 16) > 0);
				m_fzb_pages[*p] -= 0x10000;
				break;
			case 2:
				ASSERT(m_tex_pages[*p] > 0);
				m_tex_pages[*p] -= 1;
				break;
			default:break;
		}
	}
}

bool GSRendererSW::CheckTargetPages(const u32* fb_pages, const u32* zb_pages, const GSVector4i& r)
{
	bool synced = m_rl->IsSynced();

	bool fb = fb_pages != NULL;
	bool zb = zb_pages != NULL;

	bool res = false;

	if(m_fzb != m_context->offset.fzb4)
	{
		// targets changed, check everything

		m_fzb = m_context->offset.fzb4;
		m_fzb_bbox = r;

		if(fb_pages == NULL) fb_pages = m_context->offset.fb->GetPages(r);
		if(zb_pages == NULL) zb_pages = m_context->offset.zb->GetPages(r);

		memset(m_fzb_cur_pages, 0, sizeof(m_fzb_cur_pages));

		u32 used = 0;

		for(const u32* p = fb_pages; *p != GSOffset::EOP; p++)
		{
			u32 i = *p;

			u32 row = i >> 5;
			u32 col = 1 << (i & 31);

			m_fzb_cur_pages[row] |= col;

			used |= m_fzb_pages[i];
			used |= m_tex_pages[i];
		}

		for(const u32* p = zb_pages; *p != GSOffset::EOP; p++)
		{
			u32 i = *p;

			u32 row = i >> 5;
			u32 col = 1 << (i & 31);

			m_fzb_cur_pages[row] |= col;

			used |= m_fzb_pages[i];
			used |= m_tex_pages[i];
		}

		if(!synced)
		{
			if(used)
				res = true;
		}
	}
	else
	{
		// same target, only check new areas and cross-rendering between frame and z-buffer

		GSVector4i bbox = m_fzb_bbox.runion(r);

		bool check = !m_fzb_bbox.eq(bbox);

		m_fzb_bbox = bbox;

		if(check)
		{
			// drawing area is larger than previous time, check new parts only to avoid false positives (m_fzb_cur_pages guards)

			if(fb_pages == NULL) fb_pages = m_context->offset.fb->GetPages(r);
			if(zb_pages == NULL) zb_pages = m_context->offset.zb->GetPages(r);

			u32 used = 0;

			for(const u32* p = fb_pages; *p != GSOffset::EOP; p++)
			{
				u32 i = *p;

				u32 row = i >> 5;
				u32 col = 1 << (i & 31);
			
				if((m_fzb_cur_pages[row] & col) == 0)
				{
					m_fzb_cur_pages[row] |= col;

					used |= m_fzb_pages[i];
				}
			}

			for(const u32* p = zb_pages; *p != GSOffset::EOP; p++)
			{
				u32 i = *p;

				u32 row = i >> 5;
				u32 col = 1 << (i & 31);
			
				if((m_fzb_cur_pages[row] & col) == 0)
				{
					m_fzb_cur_pages[row] |= col;

					used |= m_fzb_pages[i];
				}
			}

			if(!synced)
			{
				if(used)
					res = true;
			}
		}

		if(!synced)
		{
			// chross-check frame and z-buffer pages, they cannot overlap with eachother and with previous batches in queue,
			// have to be careful when the two buffers are mutually enabled/disabled and alternating (Bully FBP/ZBP = 0x2300)

			if(fb && !res)
			{
				for(const u32* p = fb_pages; *p != GSOffset::EOP; p++)
				{
					if(m_fzb_pages[*p] & 0xffff0000)
					{
						res = true;
						break;
					}
				}
			}

			if(zb && !res)
			{
				for(const u32* p = zb_pages; *p != GSOffset::EOP; p++)
				{
					if(m_fzb_pages[*p] & 0x0000ffff)
					{
						res = true;

						break;
					}
				}
			}
		}
	}

	if(!fb && fb_pages != NULL) delete [] fb_pages;
	if(!zb && zb_pages != NULL) delete [] zb_pages;

	return res;
}

bool GSRendererSW::CheckSourcePages(SharedData* sd)
{
	if(!m_rl->IsSynced())
	{
		for(size_t i = 0; sd->m_tex[i].t != NULL; i++)
		{
			sd->m_tex[i].t->m_offset->GetPages(sd->m_tex[i].r, m_tmp_pages);

			u32* pages = m_tmp_pages; // sd->m_tex[i].t->m_pages.n;

			for(const u32* p = pages; *p != GSOffset::EOP; p++)
			{
				// TODO: 8H 4HL 4HH texture at the same place as the render target (24 bit, or 32-bit where the alpha channel is masked, Valkyrie Profile 2)

				if(m_fzb_pages[*p]) // currently being drawn to? => sync
				{
					return true;
				}
			}
		}
	}

	return false;
}

#include "GSTextureSW.h"

bool GSRendererSW::GetScanlineGlobalData(SharedData* data)
{
	GSScanlineGlobalData& gd = data->global;

	const GSDrawingEnvironment& env = m_env;
	const GSDrawingContext* context = m_context;
	const GS_PRIM_CLASS primclass = m_vt.m_primclass;

	gd.vm = m_mem.m_vm8;

	gd.fbr = context->offset.fb->pixel.row;
	gd.zbr = context->offset.zb->pixel.row;
	gd.fbc = context->offset.fb->pixel.col[0];
	gd.zbc = context->offset.zb->pixel.col[0];
	gd.fzbr = context->offset.fzb4->row;
	gd.fzbc = context->offset.fzb4->col;

	gd.sel.key = 0;

	gd.sel.fpsm = 3;
	gd.sel.zpsm = 3;
	gd.sel.atst = ATST_ALWAYS;
	gd.sel.tfx = TFX_NONE;
	gd.sel.ababcd = 0xff;
	gd.sel.prim = primclass;

	u32 fm = context->FRAME.FBMSK;
	u32 zm = context->ZBUF.ZMSK || context->TEST.ZTE == 0 ? 0xffffffff : 0;

	if(context->TEST.ZTE && context->TEST.ZTST == ZTST_NEVER)
	{
		fm = 0xffffffff;
		zm = 0xffffffff;
	}

	if(PRIM->TME)
	{
		if(GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
		{
			m_mem.m_clut.Read32(context->TEX0, env.TEXA);
		}
	}

	if(context->TEST.ATE)
	{
		if(!TryAlphaTest(fm, zm))
		{
			gd.sel.atst = context->TEST.ATST;
			gd.sel.afail = context->TEST.AFAIL;

			gd.aref = GSVector4i((int)context->TEST.AREF);

			switch(gd.sel.atst)
			{
			case ATST_LESS:
				gd.sel.atst = ATST_LEQUAL;
				gd.aref -= GSVector4i::x00000001();
				break;
			case ATST_GREATER:
				gd.sel.atst = ATST_GEQUAL;
				gd.aref += GSVector4i::x00000001();
				break;
			}
		}
	}

	bool fwrite = fm != 0xffffffff;
	bool ftest = gd.sel.atst != ATST_ALWAYS || context->TEST.DATE && context->FRAME.PSM != PSM_PSMCT24;

	bool zwrite = zm != 0xffffffff;
	bool ztest = context->TEST.ZTE && context->TEST.ZTST > ZTST_ALWAYS;
	if(!fwrite && !zwrite) return false;

	gd.sel.fwrite = fwrite;
	gd.sel.ftest = ftest;

	if(fwrite || ftest)
	{
		gd.sel.fpsm = GSLocalMemory::m_psm[context->FRAME.PSM].fmt;

		if((primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS) && m_vt.m_eq.rgba != 0xffff)
		{
			gd.sel.iip = PRIM->IIP;
		}

		if(PRIM->TME)
		{
			gd.sel.tfx = context->TEX0.TFX;
			gd.sel.tcc = context->TEX0.TCC;
			gd.sel.fst = PRIM->FST;
			gd.sel.ltf = m_vt.m_filter.opt_linear;

			if(GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
			{
				gd.sel.tlu = 1;

				gd.clut = (u32*)_aligned_malloc(sizeof(u32) * 256, 32); // FIXME: might address uninitialized data of the texture (0xCD) that is not in 0-15 range for 4-bpp formats

				memcpy(gd.clut, (const u32*)m_mem.m_clut, sizeof(u32) * GSLocalMemory::m_psm[context->TEX0.PSM].pal);
			}

			gd.sel.wms = context->CLAMP.WMS;
			gd.sel.wmt = context->CLAMP.WMT;

			if(gd.sel.tfx == TFX_MODULATE && gd.sel.tcc && m_vt.m_eq.rgba == 0xffff && m_vt.m_min.c.eq(GSVector4i(128)))
			{
				// modulate does not do anything when vertex color is 0x80

				gd.sel.tfx = TFX_DECAL;
			}

			bool mipmap = m_mipmap && IsMipMapDraw();

			GIFRegTEX0 TEX0 = m_context->GetSizeFixedTEX0(m_vt.m_min.t.xyxy(m_vt.m_max.t), m_vt.m_filter.opt_linear, mipmap);

			GSVector4i r;

			GetTextureMinMax(r, TEX0, context->CLAMP, gd.sel.ltf);

			GSTextureCacheSW::Texture* t = m_tc->Lookup(TEX0, env.TEXA);

			if(t == NULL) {ASSERT(0); return false;}

			data->SetSource(t, r, 0);

			gd.sel.tw = t->m_tw - 3;

			if(mipmap)
			{
				// TEX1.MMIN
				// 000 p
				// 001 l
				// 010 p round
				// 011 p tri
				// 100 l round
				// 101 l tri

				if(m_vt.m_lod.x > 0)
				{
					gd.sel.ltf = context->TEX1.MMIN >> 2;
				}
				else
				{
					// TODO: isbilinear(mmag) != isbilinear(mmin) && m_vt.m_lod.x <= 0 && m_vt.m_lod.y > 0
				}

				gd.sel.mmin = (context->TEX1.MMIN & 1) + 1; // 1: round, 2: tri
				gd.sel.lcm = context->TEX1.LCM;

				int mxl = std::min<int>((int)context->TEX1.MXL, 6) << 16;
				int k = context->TEX1.K << 12;

				if((int)m_vt.m_lod.x >= (int)context->TEX1.MXL)
				{
					k = (int)m_vt.m_lod.x << 16; // set lod to max level

					gd.sel.lcm = 1; // lod is constant
					gd.sel.mmin = 1; // tri-linear is meaningless
				}

				if(gd.sel.mmin == 2)
				{
					mxl--; // don't sample beyond the last level (TODO: add a dummy level instead?)
				}

				if(gd.sel.fst)
				{
					ASSERT(gd.sel.lcm == 1);
					ASSERT(((m_vt.m_min.t.uph(m_vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3); // ratchet and clank (menu)

					gd.sel.lcm = 1;
				}

				if(gd.sel.lcm)
				{
					int lod = std::max<int>(std::min<int>(k, mxl), 0);

					if(gd.sel.mmin == 1)
					{
						lod = (lod + 0x8000) & 0xffff0000; // rounding
					}

					gd.lod.i = GSVector4i(lod >> 16);
					gd.lod.f = GSVector4i(lod & 0xffff).xxxxl().xxzz();

					// TODO: lot to optimize when lod is constant
				}
				else
				{
					gd.mxl = GSVector4((float)mxl);
					gd.l = GSVector4((float)(-(0x10000 << context->TEX1.L)));
					gd.k = GSVector4((float)k);
				}

				GIFRegCLAMP MIP_CLAMP = context->CLAMP;

				GSVector4 tmin = m_vt.m_min.t;
				GSVector4 tmax = m_vt.m_max.t;

				static int s_counter = 0;

				for(int i = 1, j = std::min<int>((int)context->TEX1.MXL, 6); i <= j; i++)
				{
					const GIFRegTEX0& MIP_TEX0 = GetTex0Layer(i);

					MIP_CLAMP.MINU >>= 1;
					MIP_CLAMP.MINV >>= 1;
					MIP_CLAMP.MAXU >>= 1;
					MIP_CLAMP.MAXV >>= 1;

					m_vt.m_min.t *= 0.5f;
					m_vt.m_max.t *= 0.5f;

					GSTextureCacheSW::Texture* t = m_tc->Lookup(MIP_TEX0, env.TEXA, gd.sel.tw + 3);

					if(t == NULL) {ASSERT(0); return false;}

					GSVector4i r;

					GetTextureMinMax(r, MIP_TEX0, MIP_CLAMP, gd.sel.ltf);

					data->SetSource(t, r, i);
				}

				s_counter++;

				m_vt.m_min.t = tmin;
				m_vt.m_max.t = tmax;
			}
			else
			{
				// skip per pixel division if q is constant. Sprite uses flat
				// q, so it's always constant by primitive.
				// Note: the 'q' division was done in GSRendererSW::ConvertVertexBuffer
				gd.sel.fst |= (m_vt.m_eq.q || primclass == GS_SPRITE_CLASS);

				if(gd.sel.ltf && gd.sel.fst)
				{
					// if q is constant we can do the half pel shift for bilinear sampling on the vertices

					// TODO: but not when mipmapping is used!!!

					GSVector4 half(0x8000, 0x8000);

					GSVertexSW* RESTRICT v = data->vertex;

					for(int i = 0, j = data->vertex_count; i < j; i++)
					{
						GSVector4 t = v[i].t;

						v[i].t = (t - half).xyzw(t);
					}
				}
			}

			u16 tw = 1u << TEX0.TW;
			u16 th = 1u << TEX0.TH;

			switch(context->CLAMP.WMS)
			{
			case CLAMP_REPEAT:
				gd.t.min.U16[0] = gd.t.minmax.U16[0] = tw - 1;
				gd.t.max.U16[0] = gd.t.minmax.U16[2] = 0;
				gd.t.mask.U32[0] = 0xffffffff;
				break;
			case CLAMP_CLAMP:
				gd.t.min.U16[0] = gd.t.minmax.U16[0] = 0;
				gd.t.max.U16[0] = gd.t.minmax.U16[2] = tw - 1;
				gd.t.mask.U32[0] = 0;
				break;
			case CLAMP_REGION_CLAMP:
				gd.t.min.U16[0] = gd.t.minmax.U16[0] = std::min<u16>(context->CLAMP.MINU, tw - 1);
				gd.t.max.U16[0] = gd.t.minmax.U16[2] = std::min<u16>(context->CLAMP.MAXU, tw - 1);
				gd.t.mask.U32[0] = 0;
				break;
			case CLAMP_REGION_REPEAT:
				gd.t.min.U16[0] = gd.t.minmax.U16[0] = context->CLAMP.MINU & (tw - 1);
				gd.t.max.U16[0] = gd.t.minmax.U16[2] = context->CLAMP.MAXU & (tw - 1);
				gd.t.mask.U32[0] = 0xffffffff;
				break;
			default:
				break;
			}

			switch(context->CLAMP.WMT)
			{
			case CLAMP_REPEAT:
				gd.t.min.U16[4] = gd.t.minmax.U16[1] = th - 1;
				gd.t.max.U16[4] = gd.t.minmax.U16[3] = 0;
				gd.t.mask.U32[2] = 0xffffffff;
				break;
			case CLAMP_CLAMP:
				gd.t.min.U16[4] = gd.t.minmax.U16[1] = 0;
				gd.t.max.U16[4] = gd.t.minmax.U16[3] = th - 1;
				gd.t.mask.U32[2] = 0;
				break;
			case CLAMP_REGION_CLAMP:
				gd.t.min.U16[4] = gd.t.minmax.U16[1] = std::min<u16>(context->CLAMP.MINV, th - 1);
				gd.t.max.U16[4] = gd.t.minmax.U16[3] = std::min<u16>(context->CLAMP.MAXV, th - 1); // ffx anima summon scene, when the anchor appears (th = 256, maxv > 256)
				gd.t.mask.U32[2] = 0;
				break;
			case CLAMP_REGION_REPEAT:
				gd.t.min.U16[4] = gd.t.minmax.U16[1] = context->CLAMP.MINV & (th - 1); // skygunner main menu water texture 64x64, MINV = 127
				gd.t.max.U16[4] = gd.t.minmax.U16[3] = context->CLAMP.MAXV & (th - 1);
				gd.t.mask.U32[2] = 0xffffffff;
				break;
			default:
				break;
			}

			gd.t.min = gd.t.min.xxxxlh();
			gd.t.max = gd.t.max.xxxxlh();
			gd.t.mask = gd.t.mask.xxzz();
			gd.t.invmask = ~gd.t.mask;
		}

		if(PRIM->FGE)
		{
			gd.sel.fge = 1;

			gd.frb = env.FOGCOL.U32[0] & 0x00ff00ff;
			gd.fga = (env.FOGCOL.U32[0] >> 8) & 0x00ff00ff;
		}

		if(context->FRAME.PSM != PSM_PSMCT24)
		{
			gd.sel.date = context->TEST.DATE;
			gd.sel.datm = context->TEST.DATM;
		}

		if(!IsOpaque())
		{
			gd.sel.abe = PRIM->ABE;
			gd.sel.ababcd = context->ALPHA.U32[0];

			if(env.PABE.PABE)
			{
				gd.sel.pabe = 1;
			}

			if(m_aa1 && PRIM->AA1 && (primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS))
			{
				gd.sel.aa1 = 1;
			}

			gd.afix = GSVector4i((int)context->ALPHA.FIX << 7).xxzzlh();
		}

		if(gd.sel.date
		|| gd.sel.aba == 1 || gd.sel.abb == 1 || gd.sel.abc == 1 || gd.sel.abd == 1
		|| gd.sel.atst != ATST_ALWAYS && gd.sel.afail == AFAIL_RGB_ONLY
		|| gd.sel.fpsm == 0 && fm != 0 && fm != 0xffffffff
		|| gd.sel.fpsm == 1 && (fm & 0x00ffffff) != 0 && (fm & 0x00ffffff) != 0x00ffffff
		|| gd.sel.fpsm == 2 && (fm & 0x80f8f8f8) != 0 && (fm & 0x80f8f8f8) != 0x80f8f8f8)
		{
			gd.sel.rfb = 1;
		}

		gd.sel.colclamp = env.COLCLAMP.CLAMP;
		gd.sel.fba = context->FBA.FBA;

		if(env.DTHE.DTHE)
		{
			gd.sel.dthe = 1;

			gd.dimx = (GSVector4i*)_aligned_malloc(sizeof(env.dimx), 32);

			memcpy(gd.dimx, env.dimx, sizeof(env.dimx));
		}
	}

	gd.sel.zwrite = zwrite;
	gd.sel.ztest = ztest;

	if(zwrite || ztest)
	{
		u32 z_max = 0xffffffff >> (GSLocalMemory::m_psm[context->ZBUF.PSM].fmt * 8);

		gd.sel.zpsm = GSLocalMemory::m_psm[context->ZBUF.PSM].fmt;
		gd.sel.ztst = ztest ? context->TEST.ZTST : (int)ZTST_ALWAYS;
		gd.sel.zoverflow = (u32)GSVector4i(m_vt.m_max.p).z == 0x80000000U;
		gd.sel.zclamp = (u32)GSVector4i(m_vt.m_max.p).z > z_max;
	}

	#if _M_SSE >= 0x501

	gd.fm = fm;
	gd.zm = zm;

	if(gd.sel.fpsm == 1)
	{
		gd.fm |= 0xff000000;
	}
	else if(gd.sel.fpsm == 2)
	{
		u32 rb = gd.fm & 0x00f800f8;
		u32 ga = gd.fm & 0x8000f800;

		gd.fm = (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3) | 0xffff0000;
	}

	if(gd.sel.zpsm == 1)
	{
		gd.zm |= 0xff000000;
	}
	else if(gd.sel.zpsm == 2)
	{
		gd.zm |= 0xffff0000;
	}

	#else

	gd.fm = GSVector4i(fm);
	gd.zm = GSVector4i(zm);

	if(gd.sel.fpsm == 1)
	{
		gd.fm |= GSVector4i::xff000000();
	}
	else if(gd.sel.fpsm == 2)
	{
		GSVector4i rb = gd.fm & 0x00f800f8;
		GSVector4i ga = gd.fm & 0x8000f800;

		gd.fm = (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3) | GSVector4i::xffff0000();
	}

	if(gd.sel.zpsm == 1)
	{
		gd.zm |= GSVector4i::xff000000();
	}
	else if(gd.sel.zpsm == 2)
	{
		gd.zm |= GSVector4i::xffff0000();
	}

	#endif

	if(gd.sel.prim == GS_SPRITE_CLASS && !gd.sel.ftest && !gd.sel.ztest && data->bbox.eq(data->bbox.rintersect(data->scissor))) // TODO: check scissor horizontally only
	{
		gd.sel.notest = 1;

		u32 ofx = context->XYOFFSET.OFX;

		for(int i = 0, j = m_vertex.tail; i < j; i++)
		{
			#if _M_SSE >= 0x501
			if((((m_vertex.buff[i].XYZ.X - ofx) + 15) >> 4) & 7) // aligned to 8
			#else
			if((((m_vertex.buff[i].XYZ.X - ofx) + 15) >> 4) & 3) // aligned to 4
			#endif
			{
				gd.sel.notest = 0;
			
				break;
			}
		}
	}

	return true;
}

GSRendererSW::SharedData::SharedData(GSRendererSW* parent)
	: m_parent(parent)
	, m_fb_pages(NULL)
	, m_zb_pages(NULL)
	, m_fpsm(0)
	, m_zpsm(0)
	, m_using_pages(false)
	, m_syncpoint(SyncNone)
{
	m_tex[0].t = NULL;

	global.sel.key = 0;

	global.clut = NULL;
	global.dimx = NULL;
}

GSRendererSW::SharedData::~SharedData()
{
	ReleasePages();

	if(global.clut) _aligned_free(global.clut);
	if(global.dimx) _aligned_free(global.dimx);
}

//static TransactionScope::Lock s_lock;

void GSRendererSW::SharedData::UsePages(const u32* fb_pages, int fpsm, const u32* zb_pages, int zpsm)
{
	if(m_using_pages) return;

	{
		//TransactionScope scope(s_lock);

		if(global.sel.fb && fb_pages != NULL)
		{
			m_parent->UsePages(fb_pages, 0);
		}

		if(global.sel.zb && zb_pages != NULL)
		{
			m_parent->UsePages(zb_pages, 1);
		}

		for(size_t i = 0; m_tex[i].t != NULL; i++)
		{
			m_parent->UsePages(m_tex[i].t->m_pages.n, 2);
		}
	}

	m_fb_pages = fb_pages;
	m_zb_pages = zb_pages;
	m_fpsm = fpsm;
	m_zpsm = zpsm;

	m_using_pages = true;
}

void GSRendererSW::SharedData::ReleasePages()
{
	if(!m_using_pages) return;

	{
		//TransactionScope scope(s_lock);

		if(global.sel.fb)
		{
			m_parent->ReleasePages(m_fb_pages, 0);
		}

		if(global.sel.zb)
		{
			m_parent->ReleasePages(m_zb_pages, 1);
		}

		for(size_t i = 0; m_tex[i].t != NULL; i++)
		{
			m_parent->ReleasePages(m_tex[i].t->m_pages.n, 2);
		}
	}

	delete [] m_fb_pages;
	delete [] m_zb_pages;

	m_fb_pages = NULL;
	m_zb_pages = NULL;

	m_using_pages = false;
}

void GSRendererSW::SharedData::SetSource(GSTextureCacheSW::Texture* t, const GSVector4i& r, int level)
{
	ASSERT(m_tex[level].t == NULL);

	m_tex[level].t = t;
	m_tex[level].r = r;

	m_tex[level + 1].t = NULL;
}

void GSRendererSW::SharedData::UpdateSource()
{
	for(size_t i = 0; m_tex[i].t != NULL; i++)
	{
		if(m_tex[i].t->Update(m_tex[i].r))
		{
			global.tex[i] = m_tex[i].t->m_buff;
		}
		else
		{
			/* Out of memory, texturing temporarily disabled */
			global.sel.tfx = TFX_NONE;
		}
	}
}
