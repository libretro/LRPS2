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

#include "stdafx.h"
#include "GSdx.h"
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

#include "Window/GSWndRetro.h"
//#include "options.h"
#include "options_tools.h"

#define PS2E_LT_GS 0x01
#define PS2E_GS_VERSION 0x0006
#define PS2E_X86 0x01   // 32 bit
#define PS2E_X86_64 0x02   // 64 bit

static GSRenderer* s_gs = NULL;
static void (*s_irq)() = NULL;
static uint8* s_basemem = NULL;
static int s_vsync = 0;

EXPORT_C_(uint32) PS2EgetLibType()
{
	return PS2E_LT_GS;
}

EXPORT_C_(const char*) PS2EgetLibName()
{
	return GSUtil::GetLibName();
}

EXPORT_C_(uint32) PS2EgetLibVersion2(uint32 type)
{
	const uint32 revision = 1;
	const uint32 build = 2;

	return (build << 0) | (revision << 8) | (PS2E_GS_VERSION << 16) | (PLUGIN_VERSION << 24);
}

EXPORT_C_(uint32) PS2EgetCpuPlatform()
{
#ifdef _M_AMD64

	return PS2E_X86_64;

#else

	return PS2E_X86;

#endif
}

EXPORT_C GSsetBaseMem(uint8* mem)
{
	s_basemem = mem;

	if(s_gs)
	{
		s_gs->SetRegsMem(s_basemem);
	}
}

EXPORT_C GSsetSettingsDir(const char* dir)
{
	theApp.SetConfigDir(dir);
}

EXPORT_C_(void) GSsetLogDir(const char *dir)
{
}

EXPORT_C_(int) GSinit()
{
	if(!GSUtil::CheckSSE())
	{
		return -1;
	}

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

static int _GSopen(void** dsp, const char* title, GSRendererType renderer, int threads = -1)
{
	GSDevice* dev = NULL;
	bool old_api = *dsp == NULL;
	// Fresh start up or config file changed
	if(renderer == GSRendererType::Undefined)
	{
		renderer = static_cast<GSRendererType>(theApp.GetConfigI("Renderer"));
#ifdef _WIN32
		if (renderer == GSRendererType::Default)
			renderer = GSUtil::GetBestRenderer();
#endif
	}

	if(threads == -1)
	{
		threads = theApp.GetConfigI("extrathreads");
	}

	try
	{
		if (theApp.GetCurrentRendererType() != renderer)
		{
			// Emulator has made a render change request, which requires a completely
			// new s_gs -- if the emu doesn't save/restore the GS state across this
			// GSopen call then they'll get corrupted graphics, but that's not my problem.

			delete s_gs;

			s_gs = NULL;

			theApp.SetCurrentRendererType(renderer);
		}

		std::shared_ptr<GSWnd> window;
		{
			// Select the window first to detect the GL requirement
			std::vector<std::shared_ptr<GSWnd>> wnds;
			switch (renderer)
			{
				case GSRendererType::OGL_HW:
				case GSRendererType::OGL_SW:
					wnds.push_back(std::make_shared<GSWndRetroGL>());
					break;
				default:
					wnds.push_back(std::make_shared<GSWndRetro>());
					break;
			}
			int w = theApp.GetConfigI("ModeWidth");
			int h = theApp.GetConfigI("ModeHeight");
#if defined(__unix__)
			void *win_handle = (void*)((uptr*)(dsp)+1);
#else
			void *win_handle = *dsp;
#endif

			for(auto& wnd : wnds)
			{
				try
				{
					if (old_api)
					{
						// old-style API expects us to create and manage our own window:
						wnd->Create(title, w, h);
						*dsp = wnd->GetDisplay();
					}

					window = wnd; // Previous code will throw if window isn't supported

					break;
				}
				catch (GSDXRecoverableError)
				{
				}
			}

			if(!window)
			{
				GSclose();

				return -1;
			}
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

		printf("Current Renderer: %s\n", renderer_name.c_str());
		log_cb(RETRO_LOG_INFO, "Launching with Renderer:%s\n", renderer_name.c_str());

		if (dev == NULL)
		{
			return -1;
		}

		if (s_gs == NULL)
		{
			switch (renderer)
			{
			default:
#ifdef _WIN32
			case GSRendererType::DX1011_HW:
				s_gs = (GSRenderer*)new GSRendererDX11();
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

		s_gs->m_wnd = window;
	}
	catch (std::exception& ex)
	{
		// Allowing std exceptions to escape the scope of the plugin callstack could
		// be problematic, because of differing typeids between DLL and EXE compilations.
		// ('new' could throw std::alloc)

		printf("GSdx error: Exception caught in GSopen: %s", ex.what());

		return -1;
	}

	s_gs->SetRegsMem(s_basemem);
	s_gs->SetIrqCallback(s_irq);
	s_gs->SetVSync(s_vsync);

	if(!old_api)
		s_gs->SetMultithreaded(true);

	if(!s_gs->CreateDevice(dev))
	{
		// This probably means the user has DX11 configured with a video card that is only DX9
		// compliant.  Cound mean drivr issues of some sort also, but to be sure, that's the most
		// common cause of device creation errors. :)  --air

		GSclose();

		return -1;
	}

	return 0;
}


void GSUpdateOptions()
{
	s_gs->UpdateRendererOptions();
}


EXPORT_C_(void) GSosdLog(const char *utf8, uint32 color)
{
}

EXPORT_C_(void) GSosdMonitor(const char *key, const char *value, uint32 color)
{
}

EXPORT_C_(int) GSopen2(void** dsp, uint32 flags)
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
			#ifdef _WIN32
			{
				const auto config_renderer = static_cast<GSRendererType>(
					theApp.GetConfigI("Renderer")
				);

				if (current_renderer == config_renderer)
					current_renderer = GSUtil::GetBestRenderer();
				else
					current_renderer = config_renderer;
			}
			#else
				current_renderer = GSRendererType::OGL_HW;
			#endif
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

	int retval = _GSopen(dsp, "", current_renderer);

	if (s_gs != NULL)
		s_gs->SetAspectRatio(0);	 // PCSX2 manages the aspect ratios

	return retval;
}

EXPORT_C_(int) GSopen(void** dsp, const char* title, int mt)
{
	GSRendererType renderer = GSRendererType::Default;

	// Legacy GUI expects to acquire vsync from the configuration files.

	s_vsync = theApp.GetConfigI("vsync");

	if(mt == 2)
	{
		// pcsx2 sent a switch renderer request
		mt = 1;
	}
	else
	{
		// normal init

		renderer = static_cast<GSRendererType>(theApp.GetConfigI("Renderer"));
	}

	*dsp = NULL;

	int retval = _GSopen(dsp, title, renderer);

	if(retval == 0 && s_gs)
	{
		s_gs->SetMultithreaded(!!mt);
	}

	return retval;
}

GSVector2i GSgetInternalResolution()
{
	return s_gs->GetInternalResolution();
}

EXPORT_C GSreset()
{
	try
	{
		s_gs->Reset();
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSgifSoftReset(uint32 mask)
{
	try
	{
		s_gs->SoftReset(mask);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSwriteCSR(uint32 csr)
{
	try
	{
		s_gs->WriteCSR(csr);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSinitReadFIFO(uint8* mem)
{
	GL_PERF("Init Read FIFO1");
	try
	{
		s_gs->InitReadFIFO(mem, 1);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C GSreadFIFO(uint8* mem)
{
	try
	{
		s_gs->ReadFIFO(mem, 1);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C GSinitReadFIFO2(uint8* mem, uint32 size)
{
	GL_PERF("Init Read FIFO2");
	try
	{
		s_gs->InitReadFIFO(mem, size);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C GSreadFIFO2(uint8* mem, uint32 size)
{
	try
	{
		s_gs->ReadFIFO(mem, size);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C GSgifTransfer(const uint8* mem, uint32 size)
{
	try
	{
		s_gs->Transfer<3>(mem, size);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSgifTransfer1(uint8* mem, uint32 addr)
{
	try
	{
		s_gs->Transfer<0>(const_cast<uint8*>(mem) + addr, (0x4000 - addr) / 16);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSgifTransfer2(uint8* mem, uint32 size)
{
	try
	{
		s_gs->Transfer<1>(const_cast<uint8*>(mem), size);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSgifTransfer3(uint8* mem, uint32 size)
{
	try
	{
		s_gs->Transfer<2>(const_cast<uint8*>(mem), size);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSvsync(int field)
{
   s_gs->VSync(field);
}

EXPORT_C_(void) GSchangeSaveState(int, const char *filename)
{
}

EXPORT_C_(void) GSmakeSnapshot2(char *pathname, int *snapdone, int savejpg)
{
}

EXPORT_C_(uint32) GSmakeSnapshot(char* path)
{
	try
	{
		std::string s{path};

		if(!s.empty() && s[s.length() - 1] != DIRECTORY_SEPARATOR)
		{
			s = s + DIRECTORY_SEPARATOR;
		}

		return s_gs->MakeSnapshot(s + "gsdx");
	}
	catch (GSDXRecoverableError)
	{
		return false;
	}
}

EXPORT_C_(int) GSfreeze(int mode, GSFreezeData* data)
{
	try
	{
		if(mode == FREEZE_SAVE)
		{
			return s_gs->Freeze(data, false);
		}
		else if(mode == FREEZE_SIZE)
		{
			return s_gs->Freeze(data, true);
		}
		else if(mode == FREEZE_LOAD)
		{
			return s_gs->Defrost(data);
		}
	}
	catch (GSDXRecoverableError)
	{
	}

	return 0;
}

EXPORT_C GSconfigure()
{
   if(!GSUtil::CheckSSE())
      return;

   theApp.Init();
}

EXPORT_C_(int) GStest()
{
	if(!GSUtil::CheckSSE())
		return -1;

	return 0;
}

EXPORT_C GSabout()
{
}

EXPORT_C GSirqCallback(void (*irq)())
{
	s_irq = irq;

	if(s_gs)
	{
		s_gs->SetIrqCallback(s_irq);
	}
}

void pt(const char* str){
	struct tm *current;
	time_t now;
	
	time(&now);
	current = localtime(&now);

	printf("%02i:%02i:%02i%s", current->tm_hour, current->tm_min, current->tm_sec, str);
}

EXPORT_C_(std::wstring*) GSsetupRecording(int start)
{
   return nullptr;
}

EXPORT_C GSsetGameCRC(uint32 crc, int options)
{
	s_gs->SetGameCRC(crc, options);
}

EXPORT_C GSgetLastTag(uint32* tag)
{
	s_gs->GetLastTag(tag);
}

EXPORT_C GSgetTitleInfo2(char* dest, size_t length)
{
}

EXPORT_C GSsetFrameSkip(int frameskip)
{
	s_gs->SetFrameSkip(frameskip);
}

EXPORT_C GSsetVsync(int vsync)
{
	s_vsync = vsync;

	if(s_gs)
	{
		s_gs->SetVSync(s_vsync);
	}
}

EXPORT_C GSsetExclusive(int enabled)
{
	if(s_gs)
	{
		s_gs->SetVSync(s_vsync);
	}
}
