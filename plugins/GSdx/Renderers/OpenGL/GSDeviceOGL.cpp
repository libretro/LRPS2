/*
 *	Copyright (C) 2011-2016 PCSX2 Dev Team
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

#include "stdafx.h"
#include "GSState.h"
#include "GSDeviceOGL.h"
#include "GLState.h"
#include "GSUtil.h"
#include <fstream>

//#define ONLY_LINES

#ifdef _WIN32
#include "resource.h"
#else
#include "GSdxResources.h"
#endif

// TODO port those value into PerfMon API
#ifdef ENABLE_OGL_DEBUG_MEM_BW
uint64 g_real_texture_upload_byte = 0;
uint64 g_vertex_upload_byte = 0;
uint64 g_uniform_upload_byte = 0;
#endif

static const uint32 g_merge_cb_index      = 10;
static const uint32 g_interlace_cb_index  = 11;
static const uint32 g_convert_index       = 15;
static const uint32 g_vs_cb_index         = 20;
static const uint32 g_ps_cb_index         = 21;

bool  GSDeviceOGL::m_debug_gl_call = false;
int   GSDeviceOGL::m_shader_inst = 0;
int   GSDeviceOGL::m_shader_reg  = 0;
FILE* GSDeviceOGL::m_debug_gl_file = NULL;

GSDeviceOGL::GSDeviceOGL()
	: m_force_texture_clear(0)
	, m_fbo(0)
	, m_fbo_read(0)
	, m_va(NULL)
	, m_apitrace(0)
	, m_palette_ss(0)
	, m_vs_cb(NULL)
	, m_ps_cb(NULL)
	, m_shader(NULL)
{
	memset(&m_merge_obj, 0, sizeof(m_merge_obj));
	memset(&m_interlace, 0, sizeof(m_interlace));
	memset(&m_convert, 0, sizeof(m_convert));
	memset(&m_fxaa, 0, sizeof(m_fxaa));
	memset(&m_date, 0, sizeof(m_date));
	memset(&m_om_dss, 0, sizeof(m_om_dss));
	memset(&m_profiler, 0 , sizeof(m_profiler));
	GLState::Clear();

	m_mipmap = theApp.GetConfigI("mipmap");
	if (theApp.GetConfigB("UserHacks"))
		m_filter = static_cast<TriFiltering>(theApp.GetConfigI("UserHacks_TriFilter"));
	else
		m_filter = TriFiltering::None;

	// Reset the debug file
	#ifdef ENABLE_OGL_DEBUG
	if (theApp.GetCurrentRendererType() == GSRendererType::OGL_SW)
		m_debug_gl_file = fopen("GSdx_opengl_debug_sw.txt","w");
	else
		m_debug_gl_file = fopen("GSdx_opengl_debug_hw.txt","w");
	#endif

	m_debug_gl_call =  theApp.GetConfigB("debug_opengl");

	m_disable_hw_gl_draw = theApp.GetConfigB("disable_hw_gl_draw");
}

GSDeviceOGL::~GSDeviceOGL()
{
	if (m_debug_gl_file) {
		fclose(m_debug_gl_file);
		m_debug_gl_file = NULL;
	}

	// If the create function wasn't called nothing to do.
	if (m_shader == NULL)
		return;

	GL_PUSH("GSDeviceOGL destructor");

	// Clean vertex buffer state
	delete m_va;

	// Clean m_merge_obj
	delete m_merge_obj.cb;

	// Clean m_interlace
	delete m_interlace.cb;

	// Clean m_convert
	delete m_convert.dss;
	delete m_convert.dss_write;
	delete m_convert.cb;

	// Clean m_fxaa
	delete m_fxaa.cb;

	// Clean m_date
	delete m_date.dss;

	// Clean various opengl allocation
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteFramebuffers(1, &m_fbo_read);

	// Delete HW FX
	delete m_vs_cb;
	delete m_ps_cb;
	glDeleteSamplers(1, &m_palette_ss);

	m_ps.clear();

	glDeleteSamplers(countof(m_ps_ss), m_ps_ss);

	for (uint32 key = 0; key < countof(m_om_dss); key++) delete m_om_dss[key];

	PboPool::Destroy();

	// Must be done after the destruction of all shader/program objects
	delete m_shader;
	m_shader = NULL;
}

GSTexture* GSDeviceOGL::CreateSurface(int type, int w, int h, int fmt)
{
	GL_PUSH("Create surface");

	// A wrapper to call GSTextureOGL, with the different kind of parameter
	GSTextureOGL* t = new GSTextureOGL(type, w, h, fmt, m_fbo_read, m_mipmap > 1 || m_filter != TriFiltering::None);

	// NOTE: I'm not sure RenderTarget always need to be cleared. It could be costly for big upscale.
	// FIXME: it will be more logical to do it in FetchSurface. This code is only called at first creation
	//  of the texture. However we could reuse a deleted texture.
	if (m_force_texture_clear == 0) {
		// Clear won't be done if the texture isn't committed. Commit the full texture to ensure
		// correct behavior of force clear option (debug option)
		t->Commit();

		switch(type)
		{
			case GSTexture::RenderTarget:
				ClearRenderTarget(t, 0);
				break;
			case GSTexture::DepthStencil:
				ClearDepth(t);
				// No need to clear the stencil now.
				break;
		}
	}

	return t;
}

GSTexture* GSDeviceOGL::FetchSurface(int type, int w, int h, int format)
{
	if (format == 0)
		format = (type == GSTexture::DepthStencil || type == GSTexture::SparseDepthStencil) ? GL_DEPTH32F_STENCIL8 : GL_RGBA8;

	GSTexture* t = GSDevice::FetchSurface(type, w, h, format);


	if (m_force_texture_clear) {
		// Clear won't be done if the texture isn't committed. Commit the full texture to ensure
		// correct behavior of force clear option (debug option)
		t->Commit();

		GSVector4 red(1.0f, 0.0f, 0.0f, 1.0f);
		switch(type)
		{
			case GSTexture::RenderTarget:
				ClearRenderTarget(t, 0);
				break;
			case GSTexture::DepthStencil:
				ClearDepth(t);
				// No need to clear the stencil now.
				break;
			case GSTexture::Texture:
				if (m_force_texture_clear > 1)
					static_cast<GSTextureOGL*>(t)->Clear((void*)&red);
				else if (m_force_texture_clear)
					static_cast<GSTextureOGL*>(t)->Clear(NULL);

				break;
		}
	}

	return t;
}

bool GSDeviceOGL::Create(const std::shared_ptr<GSWnd> &wnd)
{
	std::vector<char> shader;
	// ****************************************************************
	// Debug helper
	// ****************************************************************
#ifdef ENABLE_OGL_DEBUG
	if (theApp.GetConfigB("debug_opengl")) {
		glDebugMessageCallback((GLDEBUGPROC)DebugOutputToFile, NULL);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);

		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
		// Useless info message on Nvidia driver
		GLuint ids[] = {0x20004};
		glDebugMessageControl(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DONT_CARE, countof(ids), ids, false);
	}
#endif

	m_force_texture_clear = theApp.GetConfigI("force_texture_clear");

	// WARNING it must be done after the control setup (at least on MESA)
	GL_PUSH("GSDeviceOGL::Create");

	// ****************************************************************
	// Various object
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Various");

		m_shader = new GSShaderOGL();

		glGenFramebuffers(1, &m_fbo);
		// Always write to the first buffer
		OMSetFBO(m_fbo);
		GLenum target[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, target);
		OMSetFBO(GL_DEFAULT_FRAMEBUFFER);

		glGenFramebuffers(1, &m_fbo_read);
		// Always read from the first buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, GL_DEFAULT_FRAMEBUFFER);
	}

	// ****************************************************************
	// Vertex buffer state
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Vertex Buffer");

		static_assert(sizeof(GSVertexPT1) == sizeof(GSVertex), "wrong GSVertex size");
		std::vector<GSInputLayoutOGL> il_convert = {
			{0, 2 , GL_FLOAT          , GL_FALSE , sizeof(GSVertexPT1) , (const GLvoid*)(0) }  ,
			{1, 2 , GL_FLOAT          , GL_FALSE , sizeof(GSVertexPT1) , (const GLvoid*)(16) } ,
			{2, 4 , GL_UNSIGNED_BYTE  , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(8) }  ,
			{3, 1 , GL_FLOAT          , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(12) } ,
			{4, 2 , GL_UNSIGNED_SHORT , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(16) } ,
			{5, 1 , GL_UNSIGNED_INT   , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(20) } ,
			{6, 2 , GL_UNSIGNED_SHORT , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(24) } ,
			{7, 4 , GL_UNSIGNED_BYTE  , GL_TRUE  , sizeof(GSVertex)    , (const GLvoid*)(28) } , // Only 1 byte is useful but hardware unit only support 4B
		};
		m_va = new GSVertexBufferStateOGL(il_convert);
	}

	// ****************************************************************
	// Pre Generate the different sampler object
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Sampler");

		for (uint32 key = 0; key < countof(m_ps_ss); key++) {
			m_ps_ss[key] = CreateSampler(PSSamplerSelector(key));
		}
	}

	// ****************************************************************
	// convert
	// ****************************************************************
	GLuint vs = 0;
	GLuint ps = 0;
	{
		GL_PUSH("GSDeviceOGL::Convert");

		m_convert.cb = new GSUniformBufferOGL("Misc UBO", g_convert_index, sizeof(MiscConstantBuffer));
		// Upload once and forget about it.
		// Use value of 1 when upscale multiplier is 0 for ScalingFactor,
		// this is to avoid doing math with 0 in shader. It helps custom res be less broken.
		m_misc_cb_cache.ScalingFactor = GSVector4i(std::max(1, theApp.GetConfigI("upscale_multiplier")));
		m_convert.cb->cache_upload(&m_misc_cb_cache);

		theApp.LoadResource(IDR_CONVERT_GLSL, shader);

		vs = m_shader->Compile("convert.glsl", "vs_main", GL_VERTEX_SHADER, shader.data());

		m_convert.vs = vs;
		for(size_t i = 0; i < countof(m_convert.ps); i++) {
			ps = m_shader->Compile("convert.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, shader.data());
			std::string pretty_name = "Convert pipe " + std::to_string(i);
			m_convert.ps[i] = m_shader->LinkPipeline(pretty_name, vs, 0, ps);
		}

		PSSamplerSelector point;
		m_convert.pt = GetSamplerID(point);

		PSSamplerSelector bilinear;
		bilinear.biln = true;
		m_convert.ln = GetSamplerID(bilinear);

		m_convert.dss = new GSDepthStencilOGL();
		m_convert.dss_write = new GSDepthStencilOGL();
		m_convert.dss_write->EnableDepth();
		m_convert.dss_write->SetDepth(GL_ALWAYS, true);
	}

	// ****************************************************************
	// merge
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Merge");

		m_merge_obj.cb = new GSUniformBufferOGL("Merge UBO", g_merge_cb_index, sizeof(MergeConstantBuffer));

		theApp.LoadResource(IDR_MERGE_GLSL, shader);

		for(size_t i = 0; i < countof(m_merge_obj.ps); i++) {
			ps = m_shader->Compile("merge.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, shader.data());
			std::string pretty_name = "Merge pipe " + std::to_string(i);
			m_merge_obj.ps[i] = m_shader->LinkPipeline(pretty_name, vs, 0, ps);
		}
	}

	// ****************************************************************
	// interlace
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Interlace");

		m_interlace.cb = new GSUniformBufferOGL("Interlace UBO", g_interlace_cb_index, sizeof(InterlaceConstantBuffer));

		theApp.LoadResource(IDR_INTERLACE_GLSL, shader);

		for(size_t i = 0; i < countof(m_interlace.ps); i++) {
			ps = m_shader->Compile("interlace.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, shader.data());
			std::string pretty_name = "Interlace pipe " + std::to_string(i);
			m_interlace.ps[i] = m_shader->LinkPipeline(pretty_name, vs, 0, ps);
		}
	}

	// ****************************************************************
	// rasterization configuration
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Rasterization");

#ifdef ONLY_LINES
		glLineWidth(5.0);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
		glDisable(GL_CULL_FACE);
		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_MULTISAMPLE);
		glDisable(GL_DITHER); // Honestly I don't know!
	}

	// ****************************************************************
	// DATE
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Date");

		m_date.dss = new GSDepthStencilOGL();
		m_date.dss->EnableStencil();
		m_date.dss->SetStencil(GL_ALWAYS, GL_REPLACE);
	}

	// ****************************************************************
	// Use DX coordinate convention
	// ****************************************************************

	// VS gl_position.z => [-1,-1]
	// FS depth => [0, 1]
	// because of -1 we loose lot of precision for small GS value
	// This extension allow FS depth to range from -1 to 1. So
	// gl_position.z could range from [0, 1]
	// Change depth convention
	if (GLExtension::Has("GL_ARB_clip_control"))
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	// ****************************************************************
	// HW renderer shader
	// ****************************************************************
	CreateTextureFX();

	// ****************************************************************
	// Pbo Pool allocation
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::PBO");

		// Mesa seems to use it to compute the row length. In our case, we are
		// tightly packed so don't bother with this parameter and set it to the
		// minimum alignment (1 byte)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		PboPool::Init();
	}

	// ****************************************************************
	// Get Available Memory
	// ****************************************************************
	GLint vram[4] = {0};
	if (GLLoader::vendor_id_amd) {
		// Full vram, remove a small margin for others buffer
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, vram);
	} else if (GLExtension::Has("GL_NVX_gpu_memory_info")) {
		// GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX <= give full memory
		// Available vram
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, vram);
	} else {
		fprintf(stdout, "No extenstion supported to get available memory. Use default value !\n");
	}

	// When VRAM is at least 2GB, we set the limit to the default i.e. 3.8 GB
	// When VRAM is below 2GB, we add a factor 2 because RAM can be used. Potentially
	// low VRAM gpu can go higher but perf will be bad anyway.
	if (vram[0] > 0 && vram[0] < 1800000)
		GLState::available_vram = (int64)(vram[0]) * 1024ul * 2ul;

	fprintf(stdout, "Available VRAM/RAM:%lldMB for textures\n", GLState::available_vram >> 20u);
	// ****************************************************************
	// Finish window setup and backbuffer
	// ****************************************************************
	if(!GSDevice::Create(wnd))
		return false;

	GSVector4i rect = wnd->GetClientRect();
	Reset(rect.z, rect.w);

	// Basic to ensure structures are correctly packed
	static_assert(sizeof(VSSelector) == 4, "Wrong VSSelector size");
	static_assert(sizeof(PSSelector) == 8, "Wrong PSSelector size");
	static_assert(sizeof(PSSamplerSelector) == 4, "Wrong PSSamplerSelector size");
	static_assert(sizeof(OMDepthStencilSelector) == 4, "Wrong OMDepthStencilSelector size");
	static_assert(sizeof(OMColorMaskSelector) == 4, "Wrong OMColorMaskSelector size");

	return true;
}

void GSDeviceOGL::CreateTextureFX()
{
	GL_PUSH("GSDeviceOGL::CreateTextureFX");

	m_vs_cb = new GSUniformBufferOGL("HW VS UBO", g_vs_cb_index, sizeof(VSConstantBuffer));
	m_ps_cb = new GSUniformBufferOGL("HW PS UBO", g_ps_cb_index, sizeof(PSConstantBuffer));

	theApp.LoadResource(IDR_TFX_VGS_GLSL, m_shader_tfx_vgs);
	theApp.LoadResource(IDR_TFX_FS_GLSL, m_shader_tfx_fs);

	// warning 1 sampler by image unit. So you cannot reuse m_ps_ss...
	m_palette_ss = CreateSampler(PSSamplerSelector(0));
	glBindSampler(1, m_palette_ss);

	// Pre compile the (remaining) Geometry & Vertex Shader
	// One-Hot encoding
	memset(m_gs, 0, sizeof(m_gs));
	m_gs[1] = CompileGS(GSSelector(1));
	m_gs[2] = CompileGS(GSSelector(2));
	m_gs[4] = CompileGS(GSSelector(4));

	for (uint32 key = 0; key < countof(m_vs); key++)
		m_vs[key] = CompileVS(VSSelector(key));

	// Enable all bits for stencil operations. Technically 1 bit is
	// enough but buffer is polluted with noise. Clear will be limited
	// to the mask.
	glStencilMask(0xFF);
	for (uint32 key = 0; key < countof(m_om_dss); key++) {
		m_om_dss[key] = CreateDepthStencil(OMDepthStencilSelector(key));
	}

	// Help to debug FS in apitrace
	m_apitrace = CompilePS(PSSelector());
}

bool GSDeviceOGL::Reset(int w, int h)
{
	if(!GSDevice::Reset(w, h))
		return false;

	// Opengl allocate the backbuffer with the window. The render is done in the backbuffer when
	// there isn't any FBO. Only a dummy texture is created to easily detect when the rendering is done
	// in the backbuffer
	m_backbuffer = new GSTextureOGL(GSTextureOGL::Backbuffer, w, h, 0, m_fbo_read, false);

	return true;
}

void GSDeviceOGL::SetVSync(int vsync)
{
	m_wnd->SetVSync(vsync);
}

void GSDeviceOGL::Flip()
{
	m_wnd->Flip();
}

void GSDeviceOGL::BeforeDraw()
{
}

void GSDeviceOGL::AfterDraw()
{
}

void GSDeviceOGL::DrawPrimitive()
{
	BeforeDraw();
	m_va->DrawPrimitive();
	AfterDraw();
}

void GSDeviceOGL::DrawPrimitive(int offset, int count)
{
	BeforeDraw();
	m_va->DrawPrimitive(offset, count);
	AfterDraw();
}

void GSDeviceOGL::DrawIndexedPrimitive()
{
	BeforeDraw();
	if (!m_disable_hw_gl_draw)
		m_va->DrawIndexedPrimitive();
	AfterDraw();
}

void GSDeviceOGL::DrawIndexedPrimitive(int offset, int count)
{
	//ASSERT(offset + count <= (int)m_index.count);

	BeforeDraw();
	if (!m_disable_hw_gl_draw)
		m_va->DrawIndexedPrimitive(offset, count);
	AfterDraw();
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);
	if (T->HasBeenCleaned() && !T->IsBackbuffer())
		return;

	// Performance note: potentially T->Clear() could be used. Main purpose of
	// Clear() is to avoid the framebuffer setup cost. However, in this context,
	// the texture 't' will be set as the render target of the framebuffer and
	// therefore will require a framebuffer setup.

	// So using the old/standard path is faster/better albeit verbose.

	GL_PUSH("Clear RT %d", T->GetID());

	// TODO: check size of scissor before toggling it
	glDisable(GL_SCISSOR_TEST);

	uint32 old_color_mask = GLState::wrgba;
	OMSetColorMaskState();

	if (T->IsBackbuffer()) {
		OMSetFBO(GL_DEFAULT_FRAMEBUFFER);

		// glDrawBuffer(GL_BACK); // this is the default when there is no FB
		// 0 will select the first drawbuffer ie GL_BACK
		glClearBufferfv(GL_COLOR, 0, c.v);
	} else {
		OMSetFBO(m_fbo);
		OMAttachRt(T);

		glClearBufferfv(GL_COLOR, 0, c.v);

	}

	OMSetColorMaskState(OMColorMaskSelector(old_color_mask));

	glEnable(GL_SCISSOR_TEST);

	T->WasCleaned();
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, uint32 c)
{
	if (!t) return;

	GSVector4 color = GSVector4::rgba32(c) * (1.0f / 255);
	ClearRenderTarget(t, color);
}

void GSDeviceOGL::ClearDepth(GSTexture* t)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	GL_PUSH("Clear Depth %d", T->GetID());

	if (0 && GLLoader::found_GL_ARB_clear_texture) {
		// I don't know what the driver does but it creates
		// some slowdowns on Harry Potter PS
		// Maybe it triggers some texture relocations, or maybe
		// it clears also the stencil value (2 times slower)
		//
		// Let's disable this code for the moment.

		// Don't bother with Depth_Stencil insanity
		T->Clear(NULL);
	} else {
		OMSetFBO(m_fbo);
		// RT must be detached, if RT is too small, depth won't be fully cleared
		// AT tolenico 2 map clip bug
		OMAttachRt(NULL);
		OMAttachDs(T);

		// TODO: check size of scissor before toggling it
		glDisable(GL_SCISSOR_TEST);
		float c = 0.0f;
		if (GLState::depth_mask) {
			glClearBufferfv(GL_DEPTH, 0, &c);
		} else {
			glDepthMask(true);
			glClearBufferfv(GL_DEPTH, 0, &c);
			glDepthMask(false);
		}
		glEnable(GL_SCISSOR_TEST);
	}
}

void GSDeviceOGL::ClearStencil(GSTexture* t, uint8 c)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	GL_PUSH("Clear Stencil %d", T->GetID());

	// Keep SCISSOR_TEST enabled on purpose to reduce the size
	// of clean in DATE (impact big upscaling)
	OMSetFBO(m_fbo);
	OMAttachDs(T);
	GLint color = c;

	glClearBufferiv(GL_STENCIL, 0, &color);
}

GLuint GSDeviceOGL::CreateSampler(PSSamplerSelector sel)
{
	GL_PUSH("Create Sampler");

	GLuint sampler;
	glCreateSamplers(1, &sampler);

	// Bilinear filtering
	if (sel.biln) {
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	} else {
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	switch (static_cast<GS_MIN_FILTER>(sel.triln)) {
		case GS_MIN_FILTER::Nearest:
			// Nop based on biln
			break;
		case GS_MIN_FILTER::Linear:
			// Nop based on biln
			break;
		case GS_MIN_FILTER::Nearest_Mipmap_Nearest:
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			break;
		case GS_MIN_FILTER::Nearest_Mipmap_Linear:
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
			break;
		case GS_MIN_FILTER::Linear_Mipmap_Nearest:
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			break;
		case GS_MIN_FILTER::Linear_Mipmap_Linear:
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			break;
		default:
			break;
	}

	//glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, 0);
	//glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, 6);

	if (sel.tau)
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if (sel.tav)
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	int anisotropy = theApp.GetConfigI("MaxAnisotropy");
	if (anisotropy && sel.aniso) {
		if (GLExtension::Has("GL_ARB_texture_filter_anisotropic"))
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, (float)anisotropy);
		else if (GLExtension::Has("GL_EXT_texture_filter_anisotropic"))
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)anisotropy);
	}

	return sampler;
}

GLuint GSDeviceOGL::GetSamplerID(PSSamplerSelector ssel)
{
	return m_ps_ss[ssel];
}

GSDepthStencilOGL* GSDeviceOGL::CreateDepthStencil(OMDepthStencilSelector dssel)
{
	GSDepthStencilOGL* dss = new GSDepthStencilOGL();

	if (dssel.date)
	{
		dss->EnableStencil();
		if (dssel.date_one)
			dss->SetStencil(GL_EQUAL, GL_ZERO);
		else
			dss->SetStencil(GL_EQUAL, GL_KEEP);
	}

	if(dssel.ztst != ZTST_ALWAYS || dssel.zwe)
	{
		static const GLenum ztst[] =
		{
			GL_NEVER,
			GL_ALWAYS,
			GL_GEQUAL,
			GL_GREATER
		};
		dss->EnableDepth();
		dss->SetDepth(ztst[dssel.ztst], dssel.zwe);
	}

	return dss;
}

void GSDeviceOGL::InitPrimDateTexture(GSTexture* rt, const GSVector4i& area)
{
	const GSVector2i& rtsize = rt->GetSize();

	// Create a texture to avoid the useless clean@0
	if (m_date.t == NULL)
		m_date.t = CreateTexture(rtsize.x, rtsize.y, GL_R32I);

	// Clean with the max signed value
	int max_int = 0x7FFFFFFF;
	static_cast<GSTextureOGL*>(m_date.t)->Clear(&max_int, area);

	glBindImageTexture(2, static_cast<GSTextureOGL*>(m_date.t)->GetID(), 0, false, 0, GL_READ_WRITE, GL_R32I);
#ifdef ENABLE_OGL_DEBUG
	// Help to see the texture in apitrace
	PSSetShaderResource(2, m_date.t);
#endif
}

void GSDeviceOGL::RecycleDateTexture()
{
	if (m_date.t) {
		//static_cast<GSTextureOGL*>(m_date.t)->Save(format("/tmp/date_adv_%04ld.csv", GSState::s_n));

		Recycle(m_date.t);
		m_date.t = NULL;
	}
}

void GSDeviceOGL::Barrier(GLbitfield b)
{
	glMemoryBarrier(b);
}

GLuint GSDeviceOGL::CompileVS(VSSelector sel)
{
	std::string macro = format("#define VS_INT_FST %d\n", sel.int_fst);

	if (GLLoader::buggy_sso_dual_src)
		return m_shader->CompileShader("tfx_vgs.glsl", "vs_main", GL_VERTEX_SHADER, m_shader_tfx_vgs.data(), macro);
	else
		return m_shader->Compile("tfx_vgs.glsl", "vs_main", GL_VERTEX_SHADER, m_shader_tfx_vgs.data(), macro);
}

GLuint GSDeviceOGL::CompileGS(GSSelector sel)
{
	std::string macro = format("#define GS_POINT %d\n", sel.point)
		+ format("#define GS_LINE %d\n", sel.line);

	if (GLLoader::buggy_sso_dual_src)
		return m_shader->CompileShader("tfx_vgs.glsl", "gs_main", GL_GEOMETRY_SHADER, m_shader_tfx_vgs.data(), macro);
	else
		return m_shader->Compile("tfx_vgs.glsl", "gs_main", GL_GEOMETRY_SHADER, m_shader_tfx_vgs.data(), macro);
}

GLuint GSDeviceOGL::CompilePS(PSSelector sel)
{
	std::string macro = format("#define PS_FST %d\n", sel.fst)
		+ format("#define PS_WMS %d\n", sel.wms)
		+ format("#define PS_WMT %d\n", sel.wmt)
		+ format("#define PS_TEX_FMT %d\n", sel.tex_fmt)
		+ format("#define PS_DFMT %d\n", sel.dfmt)
		+ format("#define PS_DEPTH_FMT %d\n", sel.depth_fmt)
		+ format("#define PS_CHANNEL_FETCH %d\n", sel.channel)
		+ format("#define PS_URBAN_CHAOS_HLE %d\n", sel.urban_chaos_hle)
		+ format("#define PS_TALES_OF_ABYSS_HLE %d\n", sel.tales_of_abyss_hle)
		+ format("#define PS_TEX_IS_FB %d\n", sel.tex_is_fb)
		+ format("#define PS_INVALID_TEX0 %d\n", sel.invalid_tex0)
		+ format("#define PS_AEM %d\n", sel.aem)
		+ format("#define PS_TFX %d\n", sel.tfx)
		+ format("#define PS_TCC %d\n", sel.tcc)
		+ format("#define PS_ATST %d\n", sel.atst)
		+ format("#define PS_FOG %d\n", sel.fog)
		+ format("#define PS_CLR1 %d\n", sel.clr1)
		+ format("#define PS_FBA %d\n", sel.fba)
		+ format("#define PS_LTF %d\n", sel.ltf)
		+ format("#define PS_AUTOMATIC_LOD %d\n", sel.automatic_lod)
		+ format("#define PS_MANUAL_LOD %d\n", sel.manual_lod)
		+ format("#define PS_COLCLIP %d\n", sel.colclip)
		+ format("#define PS_DATE %d\n", sel.date)
		+ format("#define PS_TCOFFSETHACK %d\n", sel.tcoffsethack)
		+ format("#define PS_POINT_SAMPLER %d\n", sel.point_sampler)
		+ format("#define PS_BLEND_A %d\n", sel.blend_a)
		+ format("#define PS_BLEND_B %d\n", sel.blend_b)
		+ format("#define PS_BLEND_C %d\n", sel.blend_c)
		+ format("#define PS_BLEND_D %d\n", sel.blend_d)
		+ format("#define PS_IIP %d\n", sel.iip)
		+ format("#define PS_SHUFFLE %d\n", sel.shuffle)
		+ format("#define PS_READ_BA %d\n", sel.read_ba)
		+ format("#define PS_WRITE_RG %d\n", sel.write_rg)
		+ format("#define PS_FBMASK %d\n", sel.fbmask)
		+ format("#define PS_HDR %d\n", sel.hdr)
		+ format("#define PS_DITHER %d\n", sel.dither)
		+ format("#define PS_ZCLAMP %d\n", sel.zclamp)
		+ format("#define PS_PABE %d\n", sel.pabe)
	;

	if (GLLoader::buggy_sso_dual_src)
		return m_shader->CompileShader("tfx.glsl", "ps_main", GL_FRAGMENT_SHADER, m_shader_tfx_fs.data(), macro);
	else
		return m_shader->Compile("tfx.glsl", "ps_main", GL_FRAGMENT_SHADER, m_shader_tfx_fs.data(), macro);
}

// blit a texture into an offscreen buffer
GSTexture* GSDeviceOGL::CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format, int ps_shader)
{
	if (format == 0)
		format = GL_RGBA8;

	ASSERT(src);
	ASSERT(format == GL_RGBA8 || format == GL_R16UI || format == GL_R32UI);

	GSTexture* dst = CreateOffscreen(w, h, format);

	GSVector4 dRect(0, 0, w, h);

	// StretchRect will read an old target. However, the memory cache might contains
	// invalid data (for example due to SW blending).
	glTextureBarrier();

	StretchRect(src, sRect, dst, dRect, m_convert.ps[ps_shader]);

	return dst;
}

// Copy a sub part of texture (same as below but force a conversion)
void GSDeviceOGL::CopyRectConv(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, bool at_origin)
{
	ASSERT(sTex && dTex);
	if (!(sTex && dTex))
		return;

	const GLuint& sid = static_cast<GSTextureOGL*>(sTex)->GetID();
	const GLuint& did = static_cast<GSTextureOGL*>(dTex)->GetID();

	GL_PUSH(format("CopyRectConv from %d to %d", sid, did).c_str());

	dTex->CommitRegion(GSVector2i(r.z, r.w));

	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sid, 0);
	if (at_origin)
		glCopyTextureSubImage2D(did, GL_TEX_LEVEL_0, 0, 0, r.x, r.y, r.width(), r.height());
	else
		glCopyTextureSubImage2D(did, GL_TEX_LEVEL_0, r.x, r.y, r.x, r.y, r.width(), r.height());

	glBindFramebuffer(GL_READ_FRAMEBUFFER, GL_DEFAULT_FRAMEBUFFER);
}

// Copy a sub part of a texture into another
void GSDeviceOGL::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r)
{
	ASSERT(sTex && dTex);
	if (!(sTex && dTex))
		return;

	const GLuint& sid = static_cast<GSTextureOGL*>(sTex)->GetID();
	const GLuint& did = static_cast<GSTextureOGL*>(dTex)->GetID();

	GL_PUSH("CopyRect from %d to %d", sid, did);

#ifdef ENABLE_OGL_DEBUG
	PSSetShaderResource(6, sTex);
#endif

	dTex->CommitRegion(GSVector2i(r.z, r.w));

	ASSERT(GLExtension::Has("GL_ARB_copy_image") && glCopyImageSubData);
	glCopyImageSubData( sid, GL_TEXTURE_2D,
			0, r.x, r.y, 0,
			did, GL_TEXTURE_2D,
			0, 0, 0, 0,
			r.width(), r.height(), 1);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[shader], linear);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, ps, m_NO_BLEND, OMColorMaskSelector(), linear);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha)
{
	OMColorMaskSelector cms;

	cms.wr = red;
	cms.wg = green;
	cms.wb = blue;
	cms.wa = alpha;

	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[ShaderConvert_COPY], m_NO_BLEND, cms, false);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, int bs, OMColorMaskSelector cms, bool linear)
{
	if(!sTex || !dTex)
	{
		ASSERT(0);
		return;
	}

	bool draw_in_depth = (ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT32] || ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT24] ||
		ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT16] || ps == m_convert.ps[ShaderConvert_RGB5A1_TO_FLOAT16]);

	// Performance optimization. It might be faster to use a framebuffer blit for standard case
	// instead to emulate it with shader
	// see https://www.opengl.org/wiki/Framebuffer#Blitting

	GL_PUSH("StretchRect from %d to %d", sTex->GetID(), dTex->GetID());

	// ************************************
	// Init
	// ************************************

	BeginScene();

	GSVector2i ds = dTex->GetSize();

	m_shader->BindPipeline(ps);

	// ************************************
	// om
	// ************************************

	if (draw_in_depth)
		OMSetDepthStencilState(m_convert.dss_write);
	else
		OMSetDepthStencilState(m_convert.dss);

	if (draw_in_depth)
		OMSetRenderTargets(NULL, dTex);
	else
		OMSetRenderTargets(dTex, NULL);

	OMSetBlendState((uint8)bs);
	OMSetColorMaskState(cms);

	// ************************************
	// ia
	// ************************************


	// Original code from DX
	float left = dRect.x * 2 / ds.x - 1.0f;
	float right = dRect.z * 2 / ds.x - 1.0f;
#if 0
	float top = 1.0f - dRect.y * 2 / ds.y;
	float bottom = 1.0f - dRect.w * 2 / ds.y;
#else
	// Opengl get some issues with the coordinate
	// I flip top/bottom to fix scaling of the internal resolution
	float top = -1.0f + dRect.y * 2 / ds.y;
	float bottom = -1.0f + dRect.w * 2 / ds.y;
#endif

	// Flip y axis only when we render in the backbuffer
	// By default everything is render in the wrong order (ie dx).
	// 1/ consistency between several pass rendering (interlace)
	// 2/ in case some GSdx code expect thing in dx order.
	// Only flipping the backbuffer is transparent (I hope)...
	GSVector4 flip_sr = sRect;
	if (static_cast<GSTextureOGL*>(dTex)->IsBackbuffer()) {
		flip_sr.y = sRect.w;
		flip_sr.w = sRect.y;
	}

	GSVertexPT1 vertices[] =
	{
		{GSVector4(left  , top   , 0.0f, 0.0f) , GSVector2(flip_sr.x , flip_sr.y)} ,
		{GSVector4(right , top   , 0.0f, 0.0f) , GSVector2(flip_sr.z , flip_sr.y)} ,
		{GSVector4(left  , bottom, 0.0f, 0.0f) , GSVector2(flip_sr.x , flip_sr.w)} ,
		{GSVector4(right , bottom, 0.0f, 0.0f) , GSVector2(flip_sr.z , flip_sr.w)} ,
	};

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);

	// ************************************
	// Texture
	// ************************************

	PSSetShaderResource(0, sTex);
	PSSetSamplerState(linear ? m_convert.ln : m_convert.pt);

	// ************************************
	// Draw
	// ************************************
	dTex->CommitRegion(GSVector2i((int)dRect.z + 1, (int)dRect.w + 1));
	DrawPrimitive();

	// ************************************
	// End
	// ************************************

	EndScene();
}

void GSDeviceOGL::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	GL_PUSH("DoMerge");

	GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;

	// Merge the 2 source textures (sTex[0],sTex[1]). Final results go to dTex. Feedback write will go to sTex[2].
	// If either 2nd output is disabled or SLBG is 1, a background color will be used.
	// Note: background color is also used when outside of the unit rectangle area
	OMSetColorMaskState();
	ClearRenderTarget(dTex, c);

	// Upload constant to select YUV algo
	if (feedback_write_2 || feedback_write_1) {
		// Write result to feedback loop
		m_misc_cb_cache.EMOD_AC.x = EXTBUF.EMODA;
		m_misc_cb_cache.EMOD_AC.y = EXTBUF.EMODC;
		m_convert.cb->cache_upload(&m_misc_cb_cache);
	}

	if (sTex[1] && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg)) {
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		StretchRect(sTex[1], sRect[1], dTex, dRect[1], ShaderConvert_COPY);
	}

	// Save 2nd output
	if (feedback_write_2) // FIXME I'm not sure dRect[1] is always correct
		StretchRect(dTex, full_r, sTex[2], dRect[1], ShaderConvert_YUV);

	// Restore background color to process the normal merge
	if (feedback_write_2_but_blend_bg)
		ClearRenderTarget(dTex, c);

	if (sTex[0]) {
		if (PMODE.AMOD == 1) // Keep the alpha from the 2nd output
			OMSetColorMaskState(OMColorMaskSelector(0x7));

		// 1st output is enabled. It must be blended
		if (PMODE.MMOD == 1) {
			// Blend with a constant alpha
			m_merge_obj.cb->cache_upload(&c.v);
			StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge_obj.ps[1], m_MERGE_BLEND, OMColorMaskSelector());
		} else {
			// Blend with 2 * input alpha
			StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge_obj.ps[0], m_MERGE_BLEND, OMColorMaskSelector());
		}
	}

	if (feedback_write_1) // FIXME I'm not sure dRect[0] is always correct
		StretchRect(dTex, full_r, sTex[2], dRect[0], ShaderConvert_YUV);
}

void GSDeviceOGL::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	GL_PUSH("DoInterlace");

	OMSetColorMaskState();

	GSVector4 s = GSVector4(dTex->GetSize());

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;

	cb.ZrH = GSVector2(0, 1.0f / s.y);
	cb.hH = s.y / 2;

	m_interlace.cb->cache_upload(&cb);

	StretchRect(sTex, sRect, dTex, dRect, m_interlace.ps[shader], linear);
}

void GSDeviceOGL::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	// Lazy compile
	if (!m_fxaa.ps) {
		if (!GLLoader::found_GL_ARB_gpu_shader5) { // GL4.0 extension
			return;
		}

		std::string fxaa_macro = "#define FXAA_GLSL_130 1\n";
		fxaa_macro += "#extension GL_ARB_gpu_shader5 : enable\n";

		std::vector<char> shader;
		theApp.LoadResource(IDR_FXAA_FX, shader);

		GLuint ps = m_shader->Compile("fxaa.fx", "ps_main", GL_FRAGMENT_SHADER, shader.data(), fxaa_macro);
		m_fxaa.ps = m_shader->LinkPipeline("FXAA pipe", m_convert.vs, 0, ps);
	}

	GL_PUSH("DoFxaa");

	OMSetColorMaskState();

	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	StretchRect(sTex, sRect, dTex, dRect, m_fxaa.ps, true);
}

void GSDeviceOGL::SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm)
{
	GL_PUSH("DATE First Pass");

	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows

	BeginScene();

	ClearStencil(ds, 0);

	m_shader->BindPipeline(m_convert.ps[datm ? ShaderConvert_DATM_1 : ShaderConvert_DATM_0]);

	// om

	OMSetDepthStencilState(m_date.dss);
	if (GLState::blend) {
		glDisable(GL_BLEND);
	}
	OMSetRenderTargets(NULL, ds, &GLState::scissor);

	// ia

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);


	// Texture

	PSSetShaderResource(0, rt);
	PSSetSamplerState(m_convert.pt);

	DrawPrimitive();

	if (GLState::blend) {
		glEnable(GL_BLEND);
	}

	EndScene();
}

void GSDeviceOGL::EndScene()
{
	m_va->EndScene();
}

void GSDeviceOGL::IASetVertexBuffer(const void* vertices, size_t count)
{
	m_va->UploadVB(vertices, count);
}

void GSDeviceOGL::IASetIndexBuffer(const void* index, size_t count)
{
	m_va->UploadIB(index, count);
}

void GSDeviceOGL::IASetPrimitiveTopology(GLenum topology)
{
	m_va->SetTopology(topology);
}

void GSDeviceOGL::PSSetShaderResource(int i, GSTexture* sr)
{
	ASSERT(i < (int)countof(GLState::tex_unit));
	// Note: Nvidia debgger doesn't support the id 0 (ie the NULL texture)
	if (sr) {
		GLuint id = static_cast<GSTextureOGL*>(sr)->GetID();
		if (GLState::tex_unit[i] != id) {
			GLState::tex_unit[i] = id;
			glBindTextureUnit(i, id);
		}
	}
}

void GSDeviceOGL::PSSetShaderResources(GSTexture* sr0, GSTexture* sr1)
{
	PSSetShaderResource(0, sr0);
	PSSetShaderResource(1, sr1);
}

void GSDeviceOGL::PSSetSamplerState(GLuint ss)
{
	if (GLState::ps_ss != ss) {
		GLState::ps_ss = ss;
		glBindSampler(0, ss);
	}
}

void GSDeviceOGL::OMAttachRt(GSTextureOGL* rt)
{
	GLuint id;
	if (rt) {
		rt->WasAttached();
		id = rt->GetID();
	} else {
		id = 0;
	}

	if (GLState::rt != id) {
		GLState::rt = id;
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
	}
}

void GSDeviceOGL::OMAttachDs(GSTextureOGL* ds)
{
	GLuint id;
	if (ds) {
		ds->WasAttached();
		id = ds->GetID();
	} else {
		id = 0;
	}

	if (GLState::ds != id) {
		GLState::ds = id;
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, id, 0);
	}
}

void GSDeviceOGL::OMSetFBO(GLuint fbo)
{
	if (GLState::fbo != fbo) {
		GLState::fbo = fbo;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
	}
}

void GSDeviceOGL::OMSetDepthStencilState(GSDepthStencilOGL* dss)
{
	dss->SetupDepth();
	dss->SetupStencil();
}

void GSDeviceOGL::OMSetColorMaskState(OMColorMaskSelector sel)
{
	if (sel.wrgba != GLState::wrgba) {
		GLState::wrgba = sel.wrgba;

		glColorMaski(0, sel.wr, sel.wg, sel.wb, sel.wa);
	}
}

void GSDeviceOGL::OMSetBlendState(uint8 blend_index, uint8 blend_factor, bool is_blend_constant, bool accumulation_blend)
{
	if (blend_index) {
		if (!GLState::blend) {
			GLState::blend = true;
			glEnable(GL_BLEND);
		}

		if (is_blend_constant && GLState::bf != blend_factor) {
			GLState::bf = blend_factor;
			float bf = (float)blend_factor / 128.0f;
			glBlendColor(bf, bf, bf, bf);
		}

		HWBlend b = GetBlend(blend_index);
		if (accumulation_blend) {
			b.src = GL_ONE;
			b.dst = GL_ONE;
		}

		if (GLState::eq_RGB != b.op) {
			GLState::eq_RGB = b.op;
			glBlendEquationSeparate(b.op, GL_FUNC_ADD);
		}

		if (GLState::f_sRGB != b.src || GLState::f_dRGB != b.dst) {
			GLState::f_sRGB = b.src;
			GLState::f_dRGB = b.dst;
			glBlendFuncSeparate(b.src, b.dst, GL_ONE, GL_ZERO);
		}

	} else {
		if (GLState::blend) {
			GLState::blend = false;
			glDisable(GL_BLEND);
		}
	}
}

void GSDeviceOGL::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor)
{
	GSTextureOGL* RT = static_cast<GSTextureOGL*>(rt);
	GSTextureOGL* DS = static_cast<GSTextureOGL*>(ds);

	if (rt == NULL || !RT->IsBackbuffer()) {
		OMSetFBO(m_fbo);
		if (rt) {
			OMAttachRt(RT);
		} else {
			OMAttachRt();
		}

		// Note: it must be done after OMSetFBO
		if (ds)
			OMAttachDs(DS);
		else
			OMAttachDs();

	} else {
		// Render in the backbuffer
		OMSetFBO(GL_DEFAULT_FRAMEBUFFER);
	}


	GSVector2i size = rt ? rt->GetSize() : ds ? ds->GetSize() : GLState::viewport;
	if(GLState::viewport != size)
	{
		GLState::viewport = size;
		// FIXME ViewportIndexedf or ViewportIndexedfv (GL4.1)
		glViewportIndexedf(0, 0, 0, GLfloat(size.x), GLfloat(size.y));
	}

	GSVector4i r = scissor ? *scissor : GSVector4i(size).zwxy();

	if(!GLState::scissor.eq(r))
	{
		GLState::scissor = r;
		// FIXME ScissorIndexedv (GL4.1)
		glScissorIndexed(0, r.x, r.y, r.width(), r.height());
	}
}

void GSDeviceOGL::SetupCB(const VSConstantBuffer* vs_cb, const PSConstantBuffer* ps_cb)
{
	GL_PUSH("UBO");
	if(m_vs_cb_cache.Update(vs_cb)) {
		m_vs_cb->upload(vs_cb);
	}

	if(m_ps_cb_cache.Update(ps_cb)) {
		m_ps_cb->upload(ps_cb);
	}
}

void GSDeviceOGL::SetupCBMisc(const GSVector4i& channel)
{
	m_misc_cb_cache.ChannelShuffle = channel;
	m_convert.cb->cache_upload(&m_misc_cb_cache);
}

void GSDeviceOGL::SetupPipeline(const VSSelector& vsel, const GSSelector& gsel, const PSSelector& psel)
{
	GLuint ps;
	auto i = m_ps.find(psel);

	if (i == m_ps.end()) {
		ps = CompilePS(psel);
		m_ps[psel] = ps;
	} else {
		ps = i->second;
	}

	{
#if defined(_DEBUG) && 0
		// Toggling Shader is bad for the perf. Let's trace parameter that often toggle to detect
		// potential uber shader possibilities.
		static PSSelector old_psel;
		static GLuint old_ps = 0;
		std::string msg("");
#define CHECK_STATE(p) if (psel.p != old_psel.p) msg.append(" ").append(#p);

		if (old_ps != ps) {

			CHECK_STATE(tex_fmt);
			CHECK_STATE(dfmt);
			CHECK_STATE(depth_fmt);
			CHECK_STATE(aem);
			CHECK_STATE(fba);
			CHECK_STATE(fog);
			CHECK_STATE(iip);
			CHECK_STATE(date);
			CHECK_STATE(atst);
			CHECK_STATE(fst);
			CHECK_STATE(tfx);
			CHECK_STATE(tcc);
			CHECK_STATE(wms);
			CHECK_STATE(wmt);
			CHECK_STATE(ltf);
			CHECK_STATE(shuffle);
			CHECK_STATE(read_ba);
			CHECK_STATE(write_rg);
			CHECK_STATE(fbmask);
			CHECK_STATE(blend_a);
			CHECK_STATE(blend_b);
			CHECK_STATE(blend_c);
			CHECK_STATE(blend_d);
			CHECK_STATE(clr1);
			CHECK_STATE(pabe);
			CHECK_STATE(hdr);
			CHECK_STATE(colclip);
			// CHECK_STATE(channel);
			// CHECK_STATE(tcoffsethack);
			// CHECK_STATE(urban_chaos_hle);
			// CHECK_STATE(tales_of_abyss_hle);
			GL_PERF("New PS :%s", msg.c_str());
		}

		old_psel.key = psel.key;
		old_ps = ps;
#endif
	}

	if (GLLoader::buggy_sso_dual_src)
		m_shader->BindProgram(m_vs[vsel], m_gs[gsel], ps);
	else
		m_shader->BindPipeline(m_vs[vsel], m_gs[gsel], ps);
}

void GSDeviceOGL::SetupSampler(PSSamplerSelector ssel)
{
	PSSetSamplerState(m_ps_ss[ssel]);
}

GLuint GSDeviceOGL::GetPaletteSamplerID()
{
	return m_palette_ss;
}

void GSDeviceOGL::SetupOM(OMDepthStencilSelector dssel)
{
	OMSetDepthStencilState(m_om_dss[dssel]);
}

// Note: used as a callback of DebugMessageCallback. Don't change the signature
void GSDeviceOGL::DebugOutputToFile(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar *gl_message, const void* userParam)
{
	std::string message(gl_message, gl_length >= 0 ? gl_length : strlen(gl_message));
	std::string type, severity, source;
	static int sev_counter = 0;
	switch(gl_type) {
		case GL_DEBUG_TYPE_ERROR_ARB               : type = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB : type = "Deprecated bhv"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB  : type = "Undefined bhv"; break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB         : type = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB         : type = "Perf"; break;
		case GL_DEBUG_TYPE_OTHER_ARB               : type = "Oth"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP              : return; // Don't print message injected by myself
		case GL_DEBUG_TYPE_POP_GROUP               : return; // Don't print message injected by myself
		default                                    : type = "TTT"; break;
	}
	switch(gl_severity) {
		case GL_DEBUG_SEVERITY_HIGH_ARB   : severity = "High"; sev_counter++; break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB : severity = "Mid"; break;
		case GL_DEBUG_SEVERITY_LOW_ARB    : severity = "Low"; break;
		default                           : if (id == 0xFEAD)
												severity = "Cache";
											else if (id == 0xB0B0)
												severity = "REG";
											else if (id == 0xD0D0)
												severity = "EXTRA";
											break;
	}
	switch(gl_source) {
		case GL_DEBUG_SOURCE_API_ARB             : source = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB   : source = "WINDOW"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB : source = "COMPILER"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB     : source = "3rdparty"; break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB     : source = "Application"; break;
		case GL_DEBUG_SOURCE_OTHER_ARB           : source = "Others"; break;
		default                                  : source = "???"; break;
	}

#ifdef _DEBUG
	// Don't spam noisy information on the terminal
	if (gl_severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
		fprintf(stderr,"T:%s\tID:%d\tS:%s\t=> %s\n", type.c_str(), GSState::s_n, severity.c_str(), message.c_str());
	}
#else
	// Print nouveau shader compiler info
	if (GSState::s_n == 0) {
		int t, local, gpr, inst, byte;
		int status = sscanf(message.c_str(), "type: %d, local: %d, gpr: %d, inst: %d, bytes: %d",
				&t, &local, &gpr, &inst, &byte);
		if (status == 5) {
			m_shader_inst += inst;
			m_shader_reg  += gpr;
			fprintf(stderr,"T:%s\t\tS:%s\t=> %s\n", type.c_str(), severity.c_str(), message.c_str());
		}
	}
#endif

	if (m_debug_gl_file)
		fprintf(m_debug_gl_file,"T:%s\tID:%d\tS:%s\t=> %s\n", type.c_str(), GSState::s_n, severity.c_str(), message.c_str());

#ifdef _DEBUG
	if (sev_counter >= 5) {
		// Close the file to flush the content on disk before exiting.
		if (m_debug_gl_file) {
			fclose(m_debug_gl_file);
			m_debug_gl_file = NULL;
		}
		ASSERT(0);
	}
#endif
}

uint16 GSDeviceOGL::ConvertBlendEnum(uint16 generic)
{
	switch (generic)
	{
	case SRC_COLOR       : return GL_SRC_COLOR;
	case INV_SRC_COLOR   : return GL_ONE_MINUS_SRC_COLOR;
	case DST_COLOR       : return GL_DST_COLOR;
	case INV_DST_COLOR   : return GL_ONE_MINUS_DST_COLOR;
	case SRC1_COLOR      : return GL_SRC1_COLOR;
	case INV_SRC1_COLOR  : return GL_ONE_MINUS_SRC1_COLOR;
	case SRC_ALPHA       : return GL_SRC_ALPHA;
	case INV_SRC_ALPHA   : return GL_ONE_MINUS_SRC_ALPHA;
	case DST_ALPHA       : return GL_DST_ALPHA;
	case INV_DST_ALPHA   : return GL_ONE_MINUS_DST_ALPHA;
	case SRC1_ALPHA      : return GL_SRC1_ALPHA;
	case INV_SRC1_ALPHA  : return GL_ONE_MINUS_SRC1_ALPHA;
	case CONST_COLOR     : return GL_CONSTANT_COLOR;
	case INV_CONST_COLOR : return GL_ONE_MINUS_CONSTANT_COLOR;
	case CONST_ONE       : return GL_ONE;
	case CONST_ZERO      : return GL_ZERO;
	case OP_ADD          : return GL_FUNC_ADD;
	case OP_SUBTRACT     : return GL_FUNC_SUBTRACT;
	case OP_REV_SUBTRACT : return GL_FUNC_REVERSE_SUBTRACT;
	default              : ASSERT(0); return 0;
	}
}
