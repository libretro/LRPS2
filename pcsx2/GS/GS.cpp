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

#include "GS.h"
#include "GSUtil.h"
#include "Renderers/SW/GSRendererSW.h"
#include "Renderers/OpenGL/GSDeviceOGL.h"
#include "Renderers/OpenGL/GSRendererOGL.h"

#ifdef _WIN32
#include "Renderers/DX11/GSRendererDX11.h"
#include "Renderers/DX11/GSDevice11.h"
#endif

#include "options_tools.h"

static bool is_d3d                  = false;
GSRenderer* s_gs                    = NULL;
static GSRendererType m_current_renderer_type;
static bool stored_toggle_state     = false;

GSApp theApp;

GSVector4i GSClientRect(void)
{
#ifdef _WIN32
	if(is_d3d)
	{
		// For whatever reason, we need this awkward hack for 
     		// D3D right now - setting orig_w/orig_h to any value other than
 		// 640, 480 seems to cause issues on various games, 
		// like 007 Nightfire
		unsigned orig_w = 640, orig_h = 480;
		return GSVector4i(0, 0, orig_w * option_upscale_mult, orig_h * option_upscale_mult);
	}
#endif
	GSVector2i internal_res = s_gs->GetInternalResolution();
        return GSVector4i(0, 0, internal_res.x, internal_res.y);
}

int GSinit(void)
{
	// Vector instructions must be avoided when initialising GS since PCSX2
	// can crash if the CPU does not support the instruction set.
	// Initialise it here instead - it's not ideal since we have to strip the
	// const type qualifier from all the affected variables.
	theApp.Init();

	GSUtil::Init();
	GSBlock::InitVectors();
	GSClut::InitVectors();
	GSRendererSW::InitVectors();
	GSVector4i::InitVectors();
	GSVector4::InitVectors();
#if _M_SSE >= 0x500
	GSVector8::InitVectors();
#endif
#if _M_SSE >= 0x501
	GSVector8i::InitVectors();
#endif
	GSVertexTrace::InitVectors();

	if (!g_const)
		return -1;

	g_const->Init();

	return 0;
}

void GSshutdown(void)
{
	delete s_gs;
	s_gs                    = nullptr;
	m_current_renderer_type = GSRendererType::Undefined;
	stored_toggle_state     = false;
	is_d3d                  = false;
}

void GSclose(void)
{
	if(!s_gs) return;

	s_gs->ResetDevice();

	delete s_gs->m_dev;

	s_gs->m_dev = NULL;
}

static int _GSopen(GSRendererType renderer, int threads, u8 *basemem)
{
	GSDevice* dev = NULL;

	is_d3d       = false;

	if (m_current_renderer_type != renderer)
	{
		// Emulator has made a render change request, which requires a completely
		// new s_gs -- if the emu doesn't save/restore the GS state across this
		// GSopen call then they'll get corrupted graphics, but that's not my problem.

		delete s_gs;

		s_gs = NULL;

		m_current_renderer_type = renderer;
	}

	switch (renderer)
	{
		case GSRendererType::OGL_HW:
		case GSRendererType::OGL_SW:
			// Load mandatory function pointer
#define GL_EXT_LOAD(ext)     *(void**)&(ext) = (void*)hw_render.get_proc_address(#ext)
			// Load extra function pointer
#define GL_EXT_LOAD_OPT(ext) *(void**)&(ext) = (void*)hw_render.get_proc_address(#ext)
#if defined(ENABLE_GL_ARB_debug_output) && defined(GL_ARB_debug_output)
GL_EXT_LOAD_OPT(glDebugMessageControlARB);
GL_EXT_LOAD_OPT(glDebugMessageInsertARB);
GL_EXT_LOAD_OPT(glDebugMessageCallbackARB);
GL_EXT_LOAD_OPT(glGetDebugMessageLogARB);
#endif
#if defined(ENABLE_GL_ARB_draw_buffers_blend) && defined(GL_ARB_draw_buffers_blend)
GL_EXT_LOAD_OPT(glBlendEquationSeparateiARB);
GL_EXT_LOAD_OPT(glBlendFuncSeparateiARB);
#endif
#if defined(ENABLE_GL_ARB_geometry_shader4) && defined(GL_ARB_geometry_shader4)
GL_EXT_LOAD_OPT(glFramebufferTextureARB);
GL_EXT_LOAD_OPT(glFramebufferTextureLayerARB);
GL_EXT_LOAD_OPT(glFramebufferTextureFaceARB);
#endif
#if defined(ENABLE_GL_ARB_sample_locations) && defined(GL_ARB_sample_locations)
GL_EXT_LOAD_OPT(glFramebufferSampleLocationsfvARB);
GL_EXT_LOAD_OPT(glEvaluateDepthValuesARB);
#endif
#if defined(ENABLE_GL_ARB_shading_language_include) && defined(GL_ARB_shading_language_include)
GL_EXT_LOAD_OPT(glDeleteNamedStringARB);
GL_EXT_LOAD_OPT(glCompileShaderIncludeARB);
GL_EXT_LOAD_OPT(glIsNamedStringARB);
#endif
#if defined(ENABLE_GL_VERSION_1_0) && defined(GL_VERSION_1_0)
GL_EXT_LOAD_OPT(glPolygonMode);
GL_EXT_LOAD_OPT(glScissor);
GL_EXT_LOAD_OPT(glTexParameteri);
GL_EXT_LOAD_OPT(glDrawBuffer);
GL_EXT_LOAD_OPT(glClear);
GL_EXT_LOAD_OPT(glClearColor);
GL_EXT_LOAD_OPT(glClearStencil);
GL_EXT_LOAD_OPT(glClearDepth);
GL_EXT_LOAD_OPT(glStencilMask);
GL_EXT_LOAD_OPT(glColorMask);
GL_EXT_LOAD_OPT(glDepthMask);
GL_EXT_LOAD_OPT(glDisable);
GL_EXT_LOAD_OPT(glEnable);
GL_EXT_LOAD_OPT(glStencilFunc);
GL_EXT_LOAD_OPT(glStencilOp);
GL_EXT_LOAD_OPT(glDepthFunc);
GL_EXT_LOAD_OPT(glPixelStorei);
GL_EXT_LOAD_OPT(glReadBuffer);
GL_EXT_LOAD_OPT(glReadPixels);
GL_EXT_LOAD_OPT(glGetError);
GL_EXT_LOAD_OPT(glGetIntegerv);
GL_EXT_LOAD_OPT(glGetString);
GL_EXT_LOAD_OPT(glGetTexImage);
GL_EXT_LOAD_OPT(glIsEnabled);
GL_EXT_LOAD_OPT(glViewport);
#endif
#if defined(ENABLE_GL_VERSION_1_1) && defined(GL_VERSION_1_1)
GL_EXT_LOAD_OPT(glDrawArrays);
GL_EXT_LOAD_OPT(glCopyTexSubImage2D);
GL_EXT_LOAD_OPT(glTexSubImage2D);
GL_EXT_LOAD_OPT(glBindTexture);
GL_EXT_LOAD_OPT(glDeleteTextures);
GL_EXT_LOAD_OPT(glGenTextures);
GL_EXT_LOAD_OPT(glIsTexture);
#endif
#if defined(ENABLE_GL_VERSION_1_3) && defined(GL_VERSION_1_3)
GL_EXT_LOAD_OPT(glActiveTexture);
#endif
#if defined(ENABLE_GL_VERSION_1_4) && defined(GL_VERSION_1_4)
GL_EXT_LOAD_OPT(glBlendFuncSeparate);
GL_EXT_LOAD_OPT(glBlendColor);
#endif
#if defined(ENABLE_GL_VERSION_1_5) && defined(GL_VERSION_1_5)
GL_EXT_LOAD_OPT(glBindBuffer);
GL_EXT_LOAD_OPT(glDeleteBuffers);
GL_EXT_LOAD_OPT(glGenBuffers);
GL_EXT_LOAD_OPT(glIsBuffer);
GL_EXT_LOAD_OPT(glBufferData);
GL_EXT_LOAD_OPT(glBufferSubData);
GL_EXT_LOAD_OPT(glMapBuffer);
GL_EXT_LOAD_OPT(glUnmapBuffer);
#endif
#if defined(ENABLE_GL_VERSION_2_0) && defined(GL_VERSION_2_0)
GL_EXT_LOAD_OPT(glBlendEquationSeparate);
GL_EXT_LOAD_OPT(glDrawBuffers);
GL_EXT_LOAD_OPT(glStencilOpSeparate);
GL_EXT_LOAD_OPT(glStencilFuncSeparate);
GL_EXT_LOAD_OPT(glStencilMaskSeparate);
GL_EXT_LOAD_OPT(glAttachShader);
GL_EXT_LOAD_OPT(glBindAttribLocation);
GL_EXT_LOAD_OPT(glCompileShader);
GL_EXT_LOAD_OPT(glCreateProgram);
GL_EXT_LOAD_OPT(glCreateShader);
GL_EXT_LOAD_OPT(glDeleteProgram);
GL_EXT_LOAD_OPT(glDeleteShader);
GL_EXT_LOAD_OPT(glDetachShader);
GL_EXT_LOAD_OPT(glDisableVertexAttribArray);
GL_EXT_LOAD_OPT(glEnableVertexAttribArray);
GL_EXT_LOAD_OPT(glGetAttachedShaders);
GL_EXT_LOAD_OPT(glGetAttribLocation);
GL_EXT_LOAD_OPT(glGetShaderiv);
GL_EXT_LOAD_OPT(glGetShaderInfoLog);
GL_EXT_LOAD_OPT(glGetShaderSource);
GL_EXT_LOAD_OPT(glIsProgram);
GL_EXT_LOAD_OPT(glIsShader);
GL_EXT_LOAD_OPT(glLinkProgram);
GL_EXT_LOAD_OPT(glShaderSource);
GL_EXT_LOAD_OPT(glUseProgram);
GL_EXT_LOAD_OPT(glVertexAttribPointer);
#endif
#if defined(ENABLE_GL_VERSION_3_0) && defined(GL_VERSION_3_0)
GL_EXT_LOAD_OPT(glColorMaski);
GL_EXT_LOAD_OPT(glGetIntegeri_v);
GL_EXT_LOAD_OPT(glEnablei);
GL_EXT_LOAD_OPT(glDisablei);
GL_EXT_LOAD_OPT(glIsEnabledi);
GL_EXT_LOAD_OPT(glBindBufferRange);
GL_EXT_LOAD_OPT(glBindBufferBase);
GL_EXT_LOAD_OPT(glVertexAttribIPointer);
GL_EXT_LOAD_OPT(glTexParameterIiv);
GL_EXT_LOAD_OPT(glTexParameterIuiv);
GL_EXT_LOAD_OPT(glClearBufferiv);
GL_EXT_LOAD_OPT(glClearBufferuiv);
GL_EXT_LOAD_OPT(glClearBufferfv);
GL_EXT_LOAD_OPT(glClearBufferfi);
GL_EXT_LOAD_OPT(glGetStringi);
GL_EXT_LOAD_OPT(glIsRenderbuffer);
GL_EXT_LOAD_OPT(glRenderbufferStorage);
GL_EXT_LOAD_OPT(glGetRenderbufferParameteriv);
GL_EXT_LOAD_OPT(glIsFramebuffer);
GL_EXT_LOAD_OPT(glBindFramebuffer);
GL_EXT_LOAD_OPT(glDeleteFramebuffers);
GL_EXT_LOAD_OPT(glGenFramebuffers);
GL_EXT_LOAD_OPT(glCheckFramebufferStatus);
GL_EXT_LOAD_OPT(glFramebufferTexture1D);
GL_EXT_LOAD_OPT(glFramebufferTexture2D);
GL_EXT_LOAD_OPT(glFramebufferTexture3D);
GL_EXT_LOAD_OPT(glGenerateMipmap);
GL_EXT_LOAD_OPT(glRenderbufferStorageMultisample);
GL_EXT_LOAD_OPT(glFramebufferTextureLayer);
GL_EXT_LOAD_OPT(glMapBufferRange);
GL_EXT_LOAD_OPT(glFlushMappedBufferRange);
GL_EXT_LOAD_OPT(glBindVertexArray);
GL_EXT_LOAD_OPT(glDeleteVertexArrays);
GL_EXT_LOAD_OPT(glGenVertexArrays);
GL_EXT_LOAD_OPT(glIsVertexArray);
#endif
#if defined(ENABLE_GL_VERSION_3_2) && defined(GL_VERSION_3_2)
GL_EXT_LOAD_OPT(glDrawElementsBaseVertex);
GL_EXT_LOAD_OPT(glFenceSync);
GL_EXT_LOAD_OPT(glIsSync);
GL_EXT_LOAD_OPT(glDeleteSync);
GL_EXT_LOAD_OPT(glClientWaitSync);
GL_EXT_LOAD_OPT(glWaitSync);
GL_EXT_LOAD_OPT(glGetInteger64v);
GL_EXT_LOAD_OPT(glGetSynciv);
GL_EXT_LOAD_OPT(glGetInteger64i_v);
GL_EXT_LOAD_OPT(glFramebufferTexture);
GL_EXT_LOAD_OPT(glGetMultisamplefv);
#endif
#if defined(ENABLE_GL_VERSION_3_3) && defined(GL_VERSION_3_3)
GL_EXT_LOAD_OPT(glGenSamplers);
GL_EXT_LOAD_OPT(glDeleteSamplers);
GL_EXT_LOAD_OPT(glIsSampler);
GL_EXT_LOAD_OPT(glBindSampler);
GL_EXT_LOAD_OPT(glSamplerParameteri);
GL_EXT_LOAD_OPT(glSamplerParameteriv);
GL_EXT_LOAD_OPT(glSamplerParameterf);
GL_EXT_LOAD_OPT(glSamplerParameterfv);
GL_EXT_LOAD_OPT(glSamplerParameterIiv);
GL_EXT_LOAD_OPT(glSamplerParameterIuiv);
#endif
#if defined(ENABLE_GL_VERSION_4_0) && defined(GL_VERSION_4_0)
GL_EXT_LOAD_OPT(glBlendEquationSeparatei);
GL_EXT_LOAD_OPT(glBlendFuncSeparatei);
#endif
#if defined(ENABLE_GL_VERSION_4_1) && defined(GL_VERSION_4_1)
GL_EXT_LOAD_OPT(glReleaseShaderCompiler);
GL_EXT_LOAD_OPT(glShaderBinary);
GL_EXT_LOAD_OPT(glGetShaderPrecisionFormat);
GL_EXT_LOAD_OPT(glClearDepthf);
GL_EXT_LOAD_OPT(glUseProgramStages);
GL_EXT_LOAD_OPT(glActiveShaderProgram);
GL_EXT_LOAD_OPT(glCreateShaderProgramv);
GL_EXT_LOAD_OPT(glBindProgramPipeline);
GL_EXT_LOAD_OPT(glDeleteProgramPipelines);
GL_EXT_LOAD_OPT(glGenProgramPipelines);
GL_EXT_LOAD_OPT(glIsProgramPipeline);
GL_EXT_LOAD_OPT(glViewportArrayv);
GL_EXT_LOAD_OPT(glViewportIndexedf);
GL_EXT_LOAD_OPT(glViewportIndexedfv);
GL_EXT_LOAD_OPT(glScissorIndexed);
#endif
#if defined(ENABLE_GL_VERSION_4_2) && defined(GL_VERSION_4_2)
GL_EXT_LOAD_OPT(glBindImageTexture);
GL_EXT_LOAD_OPT(glMemoryBarrier);
GL_EXT_LOAD_OPT(glTexStorage2D);
#endif
#if defined(ENABLE_GL_VERSION_4_3) && defined(GL_VERSION_4_3)
GL_EXT_LOAD_OPT(glClearBufferData);
GL_EXT_LOAD_OPT(glClearBufferSubData);
GL_EXT_LOAD_OPT(glCopyImageSubData);
GL_EXT_LOAD_OPT(glFramebufferParameteri);
GL_EXT_LOAD_OPT(glShaderStorageBlockBinding);
GL_EXT_LOAD_OPT(glTexStorage2DMultisample);
GL_EXT_LOAD_OPT(glBindVertexBuffer);
GL_EXT_LOAD_OPT(glDebugMessageControl);
GL_EXT_LOAD_OPT(glDebugMessageInsert);
GL_EXT_LOAD_OPT(glDebugMessageCallback);
GL_EXT_LOAD_OPT(glGetDebugMessageLog);
GL_EXT_LOAD_OPT(glPushDebugGroup);
GL_EXT_LOAD_OPT(glPopDebugGroup);
#endif
#if defined(ENABLE_GL_VERSION_4_4) && defined(GL_VERSION_4_4)
GL_EXT_LOAD_OPT(glBufferStorage);
GL_EXT_LOAD_OPT(glClearTexImage);
GL_EXT_LOAD_OPT(glClearTexSubImage);
#endif
#if defined(ENABLE_GL_VERSION_4_5) && defined(GL_VERSION_4_5)
GL_EXT_LOAD_OPT(glClipControl);
GL_EXT_LOAD_OPT(glUnmapNamedBuffer);
GL_EXT_LOAD_OPT(glFlushMappedNamedBufferRange);
GL_EXT_LOAD_OPT(glCreateFramebuffers);
GL_EXT_LOAD_OPT(glCreateRenderbuffers);
GL_EXT_LOAD_OPT(glCreateTextures);
GL_EXT_LOAD_OPT(glTextureBuffer);
GL_EXT_LOAD_OPT(glTextureBufferRange);
GL_EXT_LOAD_OPT(glTextureStorage1D);
GL_EXT_LOAD_OPT(glTextureStorage2D);
GL_EXT_LOAD_OPT(glTextureStorage3D);
GL_EXT_LOAD_OPT(glTextureStorage2DMultisample);
GL_EXT_LOAD_OPT(glTextureStorage3DMultisample);
GL_EXT_LOAD_OPT(glTextureSubImage2D);
GL_EXT_LOAD_OPT(glCopyTextureSubImage2D);
GL_EXT_LOAD_OPT(glTextureParameterf);
GL_EXT_LOAD_OPT(glTextureParameterfv);
GL_EXT_LOAD_OPT(glTextureParameteri);
GL_EXT_LOAD_OPT(glTextureParameterIiv);
GL_EXT_LOAD_OPT(glTextureParameterIuiv);
GL_EXT_LOAD_OPT(glTextureParameteriv);
GL_EXT_LOAD_OPT(glGenerateTextureMipmap);
GL_EXT_LOAD_OPT(glBindTextureUnit);
GL_EXT_LOAD_OPT(glGetTextureImage);
GL_EXT_LOAD_OPT(glCreateVertexArrays);
GL_EXT_LOAD_OPT(glDisableVertexArrayAttrib);
GL_EXT_LOAD_OPT(glEnableVertexArrayAttrib);
GL_EXT_LOAD_OPT(glCreateSamplers);
GL_EXT_LOAD_OPT(glCreateProgramPipelines);
GL_EXT_LOAD_OPT(glGetTextureSubImage);
GL_EXT_LOAD_OPT(glTextureBarrier);
#endif

			// GL1.X mess
#ifdef __unix__
			GL_EXT_LOAD(glBlendFuncSeparate);
#endif
			// Check OpenGL requirements as soon as 
			// possible so we can switch to another
			// renderer/device
			if (!(GLLoader::check_gl_requirements()))
				return -1;
			break;
		default:
			break;
	}

	switch (renderer)
	{
		default:
#ifdef _WIN32
		case GSRendererType::DX1011_HW:
			dev = new GSDevice11();
			break;
#endif
		case GSRendererType::OGL_HW:
			dev = new GSDeviceOGL();
			break;
		case GSRendererType::OGL_SW:
			dev = new GSDeviceOGL();
			break;
	}

	if (dev == NULL)
		return -1;


	if (s_gs == NULL)
	{
		switch (renderer)
		{
			default:
#ifdef _WIN32
			case GSRendererType::DX1011_HW:
                                is_d3d       = true;
				s_gs         = (GSRenderer*)new GSRendererDX11();
				break;
#endif
			case GSRendererType::OGL_HW:
				s_gs = (GSRenderer*)new GSRendererOGL();
				break;
			case GSRendererType::OGL_SW:
				if(threads == -1)
					threads = theApp.GetConfigI("extrathreads");
				s_gs = new GSRendererSW(threads);
				break;
		}
		if (s_gs == NULL)
			return -1;
	}

	s_gs->SetRegsMem(basemem);

	if (!dev->Create())
	{
		GSclose();
		return -1;
	}

	s_gs->m_dev = dev;

	return 0;
}

int GSopen2(u32 flags, u8 *basemem)
{
	const bool toggle_state = !!(flags & 4);

	switch (hw_render.context_type)
	{
		case RETRO_HW_CONTEXT_DIRECT3D:
			m_current_renderer_type = GSRendererType::DX1011_HW;
			break;
		case RETRO_HW_CONTEXT_NONE:
			m_current_renderer_type = GSRendererType::OGL_SW;
			break;
		default:
			if (! std::strcmp(option_value(STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type), "Software"))
				m_current_renderer_type = GSRendererType::OGL_SW;
			else
				m_current_renderer_type = GSRendererType::OGL_HW;
			break;
	}
	
	if (m_current_renderer_type != GSRendererType::Undefined && stored_toggle_state != toggle_state)
	{
		// SW -> HW and HW -> SW (F9 Switch)
		switch (m_current_renderer_type)
		{
			#ifdef _WIN32
			case GSRendererType::DX1011_HW:
				m_current_renderer_type = GSRendererType::OGL_SW;
				break;
			#endif
			case GSRendererType::OGL_SW:
				m_current_renderer_type = GSRendererType::OGL_HW;
				break;
			case GSRendererType::OGL_HW:
				m_current_renderer_type = GSRendererType::OGL_SW;
				break;
			default:
				m_current_renderer_type = GSRendererType::OGL_SW;
				break;
		}
	}
	stored_toggle_state = toggle_state;

	return _GSopen(m_current_renderer_type, -1, basemem);
}

int GSfreeze(int mode, void *_data)
{
	GSFreezeData* data = (GSFreezeData*)_data;
	switch (mode)
	{
		case FREEZE_SAVE:
			return s_gs->Freeze(data, false);
		case FREEZE_SIZE:
			return s_gs->Freeze(data, true);
		case FREEZE_LOAD:
			return s_gs->Defrost(data);
	}

	return 0;
}

std::string format(const char* fmt, ...)
{
	int size;
	va_list args;

	va_start(args, fmt);
	size = vsnprintf(nullptr, 0, fmt, args) + 1;
	va_end(args);

	std::vector<char> buffer(std::max(1, size));

	va_start(args, fmt);
	vsnprintf(buffer.data(), size, fmt, args);
	va_end(args);

	return {buffer.data()};
}

GSApp::GSApp()
{
	// Empty constructor causes an illegal instruction exception on an SSE4.2 machine on Windows.
	// Non-empty doesn't, but raises a SIGILL signal when compiled against GCC 6.1.1.
	// So here's a compromise.
#ifdef _WIN32
	Init();
#endif
}

void GSApp::Init()
{
	m_current_renderer_type = GSRendererType::Undefined;

	// Init configuration map with default values
	// later in the flow they will be overwritten by custom config
	m_current_configuration["clut_load_before_draw"]     = "0";
	m_current_configuration["extrathreads"]              = "2";
	m_current_configuration["extrathreads_height"]       = "4";
	m_current_configuration["force_texture_clear"]       = "0"; /* TODO/FIXME - GL only, remove later after Burnout hack? */
	m_current_configuration["linear_present"]            = "1";
	m_current_configuration["NTSC_Saturation"]           = "1";
	m_current_configuration["paltex"]                    = "0";
	m_current_configuration["UserHacks_TriFilter"]       = std::to_string(static_cast<s8>(TriFiltering::None));
	m_current_configuration["wrap_gs_mem"]               = "0";
}

int GSApp::GetConfigI(const char* entry)
{
	return std::stoi(m_current_configuration[entry]);
}

bool GSApp::GetConfigB(const char* entry)
{
	return !!GetConfigI(entry);
}

GSRendererType GetCurrentRendererType(void)
{
	return m_current_renderer_type;
}
