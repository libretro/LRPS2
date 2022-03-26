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
#include "Renderers/Null/GSRendererNull.h"
#include "Renderers/Null/GSDeviceNull.h"
#include "Renderers/OpenGL/GSDeviceOGL.h"
#include "Renderers/OpenGL/GSRendererOGL.h"

#ifdef _WIN32
#include "Renderers/DX11/GSRendererDX11.h"
#include "Renderers/DX11/GSDevice11.h"
#endif

#include "options_tools.h"

static bool is_d3d                  = false;
static GSRenderer* s_gs             = NULL;
static u8* s_basemem             = NULL;

GSdxApp theApp;

GSVector2i GSgetInternalResolution()
{
	return s_gs->GetInternalResolution();
}

GSVector4i GSClientRect(void)
{
#ifdef _WIN32
	if(is_d3d)
	{
		// For whatever reason, we need this awkward hack right now for 
     		// D3D right now - setting orig_w/orig_h to any value other than
 		// 640, 480 seems to cause issues on various games, 
		// like 007 Nightfire
		//unsigned orig_w = GSgetInternalResolution().x / option_upscale_mult;
		//unsigned orig_h = GSgetInternalResolution().y / option_upscale_mult;
		unsigned orig_w = 640, orig_h = 480;
		return GSVector4i(0, 0, orig_w * option_upscale_mult, orig_h * option_upscale_mult);
	}
#endif
	GSVector2i internal_res = GSgetInternalResolution();
        return GSVector4i(0, 0, internal_res.x, internal_res.y);
}

EXPORT_C GSsetBaseMem(u8* mem)
{
	s_basemem = mem;

	if(s_gs)
	{
		s_gs->SetRegsMem(s_basemem);
	}
}

EXPORT_C_(int) GSinit()
{
	// Vector instructions must be avoided when initialising GSdx since PCSX2
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

	if (g_const == nullptr)
		return -1;
	else
		g_const->Init();

	return 0;
}

EXPORT_C GSshutdown()
{
	delete s_gs;
	s_gs = nullptr;

	theApp.SetCurrentRendererType(GSRendererType::Undefined);
}

EXPORT_C GSclose()
{
	if(s_gs == NULL) return;

	s_gs->ResetDevice();

	delete s_gs->m_dev;

	s_gs->m_dev = NULL;
}

static int _GSopen(const char* title, GSRendererType renderer, int threads = -1)
{
	GSDevice* dev = NULL;

	is_d3d       = false;

	if(threads == -1)
	{
		threads = theApp.GetConfigI("extrathreads");
	}

	if (theApp.GetCurrentRendererType() != renderer)
	{
		// Emulator has made a render change request, which requires a completely
		// new s_gs -- if the emu doesn't save/restore the GS state across this
		// GSopen call then they'll get corrupted graphics, but that's not my problem.

		delete s_gs;

		s_gs = NULL;

		theApp.SetCurrentRendererType(renderer);
	}

	switch (renderer)
	{
		case GSRendererType::OGL_HW:
		case GSRendererType::OGL_SW:
			// Load mandatory function pointer
#define GL_EXT_LOAD(ext)     *(void**)&(ext) = (void*)hw_render.get_proc_address(#ext)
			// Load extra function pointer
#define GL_EXT_LOAD_OPT(ext) *(void**)&(ext) = (void*)hw_render.get_proc_address(#ext)

#include "PFN_WND.h"

			// GL1.X mess
#ifdef __unix__
			GL_EXT_LOAD(glBlendFuncSeparate);
#endif
			GL_EXT_LOAD_OPT(glTexturePageCommitmentEXT);

			// Check openGL requirement as soon as possible so we can switch to another
			// renderer/device
			GLLoader::check_gl_requirements();
			break;
		default:
			break;
	}

	std::string renderer_name;

	switch (renderer)
	{
		default:
#ifdef _WIN32
		case GSRendererType::DX1011_HW:
			dev = new GSDevice11();
			renderer_name = "Direct3D 11";
			break;
#endif
		case GSRendererType::OGL_HW:
			dev = new GSDeviceOGL();
			renderer_name = "OpenGL";
			break;
		case GSRendererType::OGL_SW:
			dev = new GSDeviceOGL();
			renderer_name = "Software";
			break;
		case GSRendererType::Null:
			dev = new GSDeviceNull();
			renderer_name = "Null";
			break;
	}

	log_cb(RETRO_LOG_INFO, "Launching with Renderer:%s\n", renderer_name.c_str());

	if (dev == NULL)
		return -1;


	if (s_gs == NULL)
	{
		switch (renderer)
		{
			default:
#ifdef _WIN32
			case GSRendererType::DX1011_HW:
                                
				s_gs         = (GSRenderer*)new GSRendererDX11();
                                is_d3d       = true;
				break;
#endif
			case GSRendererType::OGL_HW:
				s_gs = (GSRenderer*)new GSRendererOGL();
				break;
			case GSRendererType::OGL_SW:
				s_gs = new GSRendererSW(threads);
				break;
			case GSRendererType::Null:
				s_gs = new GSRendererNull();
				break;
		}
		if (s_gs == NULL)
			return -1;
	}

	s_gs->SetRegsMem(s_basemem);

	if(!s_gs->CreateDevice(dev))
	{
		GSclose();
		return -1;
	}

	return 0;
}

void GSUpdateOptions()
{
	s_gs->UpdateRendererOptions();
}


EXPORT_C_(int) GSopen2(u32 flags)
{
	static bool stored_toggle_state = false;
	const bool toggle_state = !!(flags & 4);

	switch (hw_render.context_type)
	{
		case RETRO_HW_CONTEXT_DIRECT3D:
			theApp.SetCurrentRendererType(GSRendererType::DX1011_HW);
			log_cb(RETRO_LOG_INFO, "Selected Renderer: DX1011_HW\n" );
			break;
		case RETRO_HW_CONTEXT_NONE:
			theApp.SetCurrentRendererType(GSRendererType::Null);
			log_cb(RETRO_LOG_INFO, "Selected Renderer: NULL\n");
			break;
		default:
			if (! std::strcmp(option_value(STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type), "Software"))
			{
				theApp.SetCurrentRendererType(GSRendererType::OGL_SW);
				log_cb(RETRO_LOG_INFO, "Selected Renderer: OGL_SW\n");
			}
			else
			{
				theApp.SetCurrentRendererType(GSRendererType::OGL_HW);
				log_cb(RETRO_LOG_INFO, "Selected Renderer: OGL_HW\n");
			}
			break;
	}
	
	auto current_renderer = theApp.GetCurrentRendererType();

	if (current_renderer != GSRendererType::Undefined && stored_toggle_state != toggle_state)
	{
		// SW -> HW and HW -> SW (F9 Switch)
		switch (current_renderer)
		{
			#ifdef _WIN32
			case GSRendererType::DX1011_HW:
				current_renderer = GSRendererType::OGL_SW;
				break;
			#endif
			case GSRendererType::OGL_SW:
				current_renderer = GSRendererType::OGL_HW;
				break;
			case GSRendererType::OGL_HW:
				current_renderer = GSRendererType::OGL_SW;
				break;
			default:
				current_renderer = GSRendererType::OGL_SW;
				break;
		}
	}
	stored_toggle_state = toggle_state;

	return _GSopen("", current_renderer);
}

EXPORT_C GSreset()
{
	s_gs->Reset();
}

EXPORT_C GSgifSoftReset(u32 mask)
{
	s_gs->SoftReset(mask);
}

EXPORT_C GSwriteCSR(u32 csr)
{
	s_gs->WriteCSR(csr);
}

EXPORT_C GSinitReadFIFO(u8* mem)
{
	s_gs->InitReadFIFO(mem, 1);
}

EXPORT_C GSreadFIFO(u8* mem)
{
	s_gs->ReadFIFO(mem, 1);
}

EXPORT_C GSinitReadFIFO2(u8* mem, u32 size)
{
	s_gs->InitReadFIFO(mem, size);
}

EXPORT_C GSreadFIFO2(u8* mem, u32 size)
{
	s_gs->ReadFIFO(mem, size);
}

EXPORT_C GSgifTransfer(const u8* mem, u32 size)
{
	s_gs->Transfer<3>(mem, size);
}

EXPORT_C GSgifTransfer1(u8* mem, u32 addr)
{
	s_gs->Transfer<0>(const_cast<u8*>(mem) + addr, (0x4000 - addr) / 16);
}

EXPORT_C GSgifTransfer2(u8* mem, u32 size)
{
	s_gs->Transfer<1>(const_cast<u8*>(mem), size);
}

EXPORT_C GSgifTransfer3(u8* mem, u32 size)
{
	s_gs->Transfer<2>(const_cast<u8*>(mem), size);
}

EXPORT_C GSvsync(int field)
{
   s_gs->VSync(field);
}

EXPORT_C_(int) GSfreeze(int mode, GSFreezeData* data)
{
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

EXPORT_C GSsetGameCRC(u32 crc, int options)
{
	s_gs->SetGameCRC(crc, options);
}

EXPORT_C GSsetFrameSkip(int frameskip)
{
	s_gs->SetFrameSkip(frameskip);
}

std::string format(const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	int size = vsnprintf(nullptr, 0, fmt, args) + 1;
	va_end(args);

	assert(size > 0);
	std::vector<char> buffer(std::max(1, size));

	va_start(args, fmt);
	vsnprintf(buffer.data(), size, fmt, args);
	va_end(args);

	return {buffer.data()};
}

#ifdef _WIN32
void* vmalloc(size_t size, bool code)
{
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, code ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
}

void vmfree(void* ptr, size_t size)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}

static HANDLE s_fh = NULL;
static u8* s_Next[8];

void* fifo_alloc(size_t size, size_t repeat)
{
	ASSERT(s_fh == NULL);

	if (repeat >= countof(s_Next))
		return vmalloc(size * repeat, false); // Fallback to default vmalloc

	s_fh = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr);
	DWORD errorID = ::GetLastError();
	if (s_fh == NULL)
		return vmalloc(size * repeat, false); // Fallback to default vmalloc

	int mmap_segment_failed = 0;
	void* fifo = MapViewOfFile(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size);
	for (size_t i = 1; i < repeat; i++) {
		void* base = (u8*)fifo + size * i;
		s_Next[i] = (u8*)MapViewOfFileEx(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size, base);
		errorID = ::GetLastError();
		if (s_Next[i] != base) {
			mmap_segment_failed++;
			if (mmap_segment_failed > 4)
			{
				fifo_free(fifo, size, repeat);
				return vmalloc(size * repeat, false); // Fallback to default vmalloc
			}
			do {
				UnmapViewOfFile(s_Next[i]);
				s_Next[i] = 0;
			} while (--i > 0);

			fifo = MapViewOfFile(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size);
		}
	}

	return fifo;
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	ASSERT(s_fh != NULL);

	if (s_fh == NULL)
	{
		if (ptr != NULL)
			vmfree(ptr, size);
		return;
	}

	UnmapViewOfFile(ptr);

	for (size_t i = 1; i < countof(s_Next); i++) {
		if (s_Next[i] != 0) {
			UnmapViewOfFile(s_Next[i]);
			s_Next[i] = 0;
		}
	}

	CloseHandle(s_fh);
	s_fh = NULL;
}

#else

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

void* vmalloc(size_t size, bool code)
{
	size_t mask = getpagesize() - 1;

	size = (size + mask) & ~mask;

	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;

	if(code) {
		prot |= PROT_EXEC;
#ifdef _M_AMD64
		flags |= MAP_32BIT;
#endif
	}

	return mmap(NULL, size, prot, flags, -1, 0);
}

void vmfree(void* ptr, size_t size)
{
	size_t mask = getpagesize() - 1;
	size = (size + mask) & ~mask;
	munmap(ptr, size);
}

static int s_shm_fd = -1;

void* fifo_alloc(size_t size, size_t repeat)
{
	ASSERT(s_shm_fd == -1);

	const char* file_name = "/GSDX.mem";
	s_shm_fd = shm_open(file_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (s_shm_fd == -1)
		return nullptr;
	shm_unlink(file_name); // file is deleted but descriptor is still open
	ftruncate(s_shm_fd, repeat * size);
	void* fifo = mmap(nullptr, size * repeat, PROT_READ | PROT_WRITE, MAP_SHARED, s_shm_fd, 0);

	for (size_t i = 1; i < repeat; i++)
	{
		void* base = (u8*)fifo + size * i;
		u8* next = (u8*)mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, s_shm_fd, 0);
	}

	return fifo;
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	ASSERT(s_shm_fd >= 0);

	if (s_shm_fd < 0)
		return;

	munmap(ptr, size * repeat);

	close(s_shm_fd);
	s_shm_fd = -1;
}

#endif

#if !defined(_MSC_VER)

// declare linux equivalents (alignment must be power of 2 (1,2,4...2^15)

#if !defined(__USE_ISOC11) || defined(ASAN_WORKAROUND)

void* _aligned_malloc(size_t size, size_t alignment)
{
	void *ret = 0;
	posix_memalign(&ret, alignment, size);
	return ret;
}

#endif

#endif


GSdxApp::GSdxApp()
{
	// Empty constructor causes an illegal instruction exception on an SSE4.2 machine on Windows.
	// Non-empty doesn't, but raises a SIGILL signal when compiled against GCC 6.1.1.
	// So here's a compromise.
#ifdef _WIN32
	Init();
#endif
}

void GSdxApp::Init()
{
	static bool is_initialised = false;
	if (is_initialised)
		return;
	is_initialised = true;

	m_current_renderer_type = GSRendererType::Undefined;

	// Init configuration map with default values
	// later in the flow they will be overwritten by custom config

	// Avoid to clutter the ini file with useless options
#ifdef _WIN32
	m_current_configuration["dx_break_on_severity"]                       = "0";
	// D3D Blending option
	m_current_configuration["accurate_blending_unit_d3d11"]               = "1";
#endif
	m_current_configuration["aa1"]                                        = "0";
	m_current_configuration["accurate_date"]                              = "1";
	m_current_configuration["accurate_blending_unit"]                     = "1";
	m_current_configuration["AspectRatio"]                                = "1";
	m_current_configuration["autoflush_sw"]                               = "1";
	m_current_configuration["clut_load_before_draw"]                      = "0";
	m_current_configuration["crc_hack_level"]                             = std::to_string(static_cast<s8>(CRCHackLevel::Automatic));
	m_current_configuration["CrcHacksExclusions"]                         = "";
	m_current_configuration["debug_glsl_shader"]                          = "0";
	m_current_configuration["debug_opengl"]                               = "0";
	m_current_configuration["disable_hw_gl_draw"]                         = "0";
	m_current_configuration["dithering_ps2"]                              = "2";
	m_current_configuration["dump"]                                       = "0";
	m_current_configuration["extrathreads"]                               = "2";
	m_current_configuration["extrathreads_height"]                        = "4";
	m_current_configuration["filter"]                                     = std::to_string(static_cast<s8>(BiFiltering::PS2));
	m_current_configuration["force_texture_clear"]                        = "0";
	m_current_configuration["fxaa"]                                       = "0";
	m_current_configuration["interlace"]                                  = "7";
	m_current_configuration["large_framebuffer"]                          = "0";
	m_current_configuration["linear_present"]                             = "1";
	m_current_configuration["MaxAnisotropy"]                              = "0";
	m_current_configuration["mipmap"]                                     = "1";
	m_current_configuration["mipmap_hw"]                                  = std::to_string(static_cast<int>(HWMipmapLevel::Automatic));
	m_current_configuration["ModeHeight"]                                 = "480";
	m_current_configuration["ModeWidth"]                                  = "640";
	m_current_configuration["NTSC_Saturation"]                            = "1";
	m_current_configuration["override_geometry_shader"]                   = "-1";
	m_current_configuration["override_GL_ARB_compute_shader"]             = "-1";
	m_current_configuration["override_GL_ARB_copy_image"]                 = "-1";
	m_current_configuration["override_GL_ARB_clear_texture"]              = "-1";
	m_current_configuration["override_GL_ARB_clip_control"]               = "-1";
	m_current_configuration["override_GL_ARB_direct_state_access"]        = "-1";
	m_current_configuration["override_GL_ARB_draw_buffers_blend"]         = "-1";
	m_current_configuration["override_GL_ARB_gpu_shader5"]                = "-1";
	m_current_configuration["override_GL_ARB_multi_bind"]                 = "-1";
	m_current_configuration["override_GL_ARB_shader_image_load_store"]    = "-1";
	m_current_configuration["override_GL_ARB_shader_storage_buffer_object"] = "-1";
	m_current_configuration["override_GL_ARB_sparse_texture"]             = "-1";
	m_current_configuration["override_GL_ARB_sparse_texture2"]            = "-1";
	m_current_configuration["override_GL_ARB_texture_view"]               = "-1";
	m_current_configuration["override_GL_ARB_vertex_attrib_binding"]      = "-1";
	m_current_configuration["override_GL_ARB_texture_barrier"]            = "-1";
#ifdef GL_EXT_TEX_SUB_IMAGE
	m_default_configuration["override_GL_ARB_get_texture_sub_image"]      = "-1";
#endif
	m_current_configuration["paltex"]                                     = "0";
	m_current_configuration["preload_frame_with_gs_data"]                 = "0";
	m_current_configuration["Renderer"]                                   = std::to_string(static_cast<int>(GSRendererType::Default));
	m_current_configuration["resx"]                                       = "1024";
	m_current_configuration["resy"]                                       = "1024";
	m_current_configuration["save"]                                       = "0";
	m_current_configuration["savef"]                                      = "0";
	m_current_configuration["savel"]                                      = "5000";
	m_current_configuration["saven"]                                      = "0";
	m_current_configuration["savet"]                                      = "0";
	m_current_configuration["savez"]                                      = "0";
	m_current_configuration["shaderfx"]                                   = "0";
	m_current_configuration["shaderfx_conf"]                              = "shaders/GSdx_FX_Settings.ini";
	m_current_configuration["shaderfx_glsl"]                              = "shaders/GSdx.fx";
	m_current_configuration["TVShader"]                                   = "0";
	m_current_configuration["upscale_multiplier"]                         = "1";
	m_current_configuration["UserHacks"]                                  = "0";
	m_current_configuration["UserHacks_align_sprite_X"]                   = "0";
	m_current_configuration["UserHacks_AutoFlush"]                        = "0";
	m_current_configuration["UserHacks_DisableDepthSupport"]              = "0";
	m_current_configuration["UserHacks_Disable_Safe_Features"]            = "0";
	m_current_configuration["UserHacks_DisablePartialInvalidation"]       = "0";
	m_current_configuration["UserHacks_CPU_FB_Conversion"]                = "0";
	m_current_configuration["UserHacks_Half_Bottom_Override"]             = "-1";
	m_current_configuration["UserHacks_HalfPixelOffset"]                  = "0";
	m_current_configuration["UserHacks_merge_pp_sprite"]                  = "0";
	m_current_configuration["UserHacks_round_sprite_offset"]              = "0";
	m_current_configuration["UserHacks_SkipDraw"]                         = "0";
	m_current_configuration["UserHacks_SkipDraw_Offset"]                  = "0";
	m_current_configuration["UserHacks_TCOffsetX"]                        = "0";
	m_current_configuration["UserHacks_TCOffsetY"]                        = "0";
	m_current_configuration["UserHacks_TextureInsideRt"]                  = "0";
	m_current_configuration["UserHacks_TriFilter"]                        = std::to_string(static_cast<s8>(TriFiltering::None));
	m_current_configuration["UserHacks_WildHack"]                         = "0";
	m_current_configuration["wrap_gs_mem"]                                = "0";
	m_current_configuration["vsync"]                                      = "0";
}


std::string GSdxApp::GetConfigS(const char* entry)
{
	
	return m_current_configuration[entry];
}

void GSdxApp::SetConfig(const char* entry, const char* value)
{
	m_current_configuration[entry] = value;
}

int GSdxApp::GetConfigI(const char* entry)
{
	return std::stoi(m_current_configuration[entry]);

}

bool GSdxApp::GetConfigB(const char* entry)
{
	return !!GetConfigI(entry);
}

void GSdxApp::SetConfig(const char* entry, int value)
{
	char buff[32] = {0};

	sprintf(buff, "%d", value);

	SetConfig(entry, buff);
}

void GSdxApp::SetCurrentRendererType(GSRendererType type)
{
	m_current_renderer_type = type;
}

GSRendererType GSdxApp::GetCurrentRendererType() const
{
	return m_current_renderer_type;
}
