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
#include "options.h"

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

#ifdef _WIN32

bool GSWndRetroDX::Create(const std::string& title, int w, int h)
{
	m_managed = true;

	return true;
}

bool GSWndRetroDX::Attach(void* handle, bool managed)
{
	m_managed = managed;
	return true;
}

void GSWndRetroDX::Detach()
{
	m_managed = true;
}

GSVector4i GSWndRetroDX::GetClientRect()
{
	return GSVector4i(0, 0, 640 * Options::upscale_multiplier, 448 * Options::upscale_multiplier);
}

bool GSWndRetroDX::SetWindowText(const char* title)
{
	if (!m_managed)
		return false;
	return true;
}
#endif
