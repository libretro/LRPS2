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
#include "GSWnd.h"
#include "GSWndRetro.h"
#include <libretro.h>
#include "options_tools.h"

extern struct retro_hw_render_callback hw_render;
extern retro_video_refresh_t video_cb;
extern retro_environment_t environ_cb;

void GSWndGL::PopulateGlFunction()
{
	// Load mandatory function pointer
#define GL_EXT_LOAD(ext)     *(void**)&(ext) = GetProcAddress(#ext, false)
	// Load extra function pointer
#define GL_EXT_LOAD_OPT(ext) *(void**)&(ext) = GetProcAddress(#ext, true)

#include "PFN_WND.h"

	// GL1.X mess
#ifdef __unix__
	GL_EXT_LOAD(glBlendFuncSeparate);
#endif
	GL_EXT_LOAD_OPT(glTexturePageCommitmentEXT);

	// Check openGL requirement as soon as possible so we can switch to another
	// renderer/device
	GLLoader::check_gl_requirements();
}

void GSWndGL::FullContextInit()
{
	PopulateGlFunction();
}

GSWndRetroGL::GSWndRetroGL()
{
}

bool GSWndRetroGL::Create()
{
	FullContextInit();
	return true;
}

void* GSWndRetroGL::GetProcAddress(const char* name, bool opt)
{
	void* ptr = (void*)hw_render.get_proc_address(name);
	if (ptr == nullptr)
	{
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

void GSWndRetroGL::Flip()
{
	video_cb(RETRO_HW_FRAME_BUFFER_VALID, GSgetInternalResolution().x, GSgetInternalResolution().y, 0);
}


bool GSWndRetro::Create()
{
	return true;
}

GSVector4i GSWndRetro::GetClientRect()
{
	int upscale_mult = option_upscale_mult;
//	return GSVector4i(0, 0, 640 , 480);
	return GSVector4i(0, 0, 640 * upscale_mult, 480 * upscale_mult);
//	return GSVector4i(0, 0, GSgetInternalResolution().x, GSgetInternalResolution().y);
}

void GSWndRetro::Flip()
{
	video_cb(NULL, GSgetInternalResolution().x, GSgetInternalResolution().y, 0);
}
