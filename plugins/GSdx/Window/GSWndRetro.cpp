/*
 *	Copyright (C) 2007-2012 Gabest
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

#include "../stdafx.h"
#include "GSWndRetro.h"
#include <libretro.h>
#include "options_tools.h"

extern struct retro_hw_render_callback hw_render;
extern retro_video_refresh_t video_cb;
extern retro_environment_t environ_cb;

// static method
int GSWndRetroGL::SelectPlatform()
{
	return 0;
}


GSWndRetroGL::GSWndRetroGL()
{
}

void GSWndRetroGL::CreateContext(int major, int minor)
{
}

void GSWndRetroGL::AttachContext()
{
	m_ctx_attached = true;
}

void GSWndRetroGL::DetachContext()
{
	m_ctx_attached = false;
}

void GSWndRetroGL::PopulateWndGlFunction()
{
}

void GSWndRetroGL::BindAPI()
{
}

bool GSWndRetroGL::Attach(void* handle, bool managed)
{
	m_managed = managed;
	return true;
}

void GSWndRetroGL::Detach()
{
	DetachContext();
	DestroyNativeResources();
}

bool GSWndRetroGL::Create(const std::string& title, int w, int h)
{
	m_managed = true;
	FullContextInit();
	return true;
}

void* GSWndRetroGL::GetProcAddress(const char* name, bool opt)
{
	void* ptr = (void*)hw_render.get_proc_address(name);
	if (ptr == nullptr)
	{
		if (theApp.GetConfigB("debug_opengl"))
			fprintf(stderr, "Failed to find %s\n", name);

		if (!opt)
			throw GSDXRecoverableError();
	}
	return ptr;
}
GSVector2i GSgetInternalResolution();
GSVector4i GSWndRetroGL::GetClientRect()
{	
	return GSVector4i(0, 0, GSgetInternalResolution().x, GSgetInternalResolution().y);
}

void GSWndRetroGL::SetSwapInterval()
{
}

void GSWndRetroGL::Flip()
{
	video_cb(RETRO_HW_FRAME_BUFFER_VALID, GSgetInternalResolution().x, GSgetInternalResolution().y, 0);
}


void* GSWndRetroGL::CreateNativeDisplay()
{
	return nullptr;
}

void* GSWndRetroGL::CreateNativeWindow(int w, int h)
{
	return nullptr;
}

void* GSWndRetroGL::AttachNativeWindow(void* handle)
{
	return handle;
}

void GSWndRetroGL::DestroyNativeResources()
{
}

bool GSWndRetroGL::SetWindowText(const char* title)
{
	return true;
}

bool GSWndRetro::Create(const std::string& title, int w, int h)
{
	m_managed = true;

	return true;
}

bool GSWndRetro::Attach(void* handle, bool managed)
{
	m_managed = managed;
	return true;
}

void GSWndRetro::Detach()
{
	m_managed = true;
}

GSVector4i GSWndRetro::GetClientRect()
{
	int upscale_mult = option_value(INT_PCSX2_OPT_UPSCALE_MULTIPLIER, KeyOptionInt::return_type);
//	return GSVector4i(0, 0, 640 , 480);
	return GSVector4i(0, 0, 640 * upscale_mult, 480 * upscale_mult);
//	return GSVector4i(0, 0, GSgetInternalResolution().x, GSgetInternalResolution().y);
}

bool GSWndRetro::SetWindowText(const char* title)
{
	if (!m_managed)
		return false;
	return true;
}

void GSWndRetro::Flip()
{
	video_cb(NULL, GSgetInternalResolution().x, GSgetInternalResolution().y, 0);
}
