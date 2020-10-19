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
#ifdef __LIBRETRO__

#include "GSWnd.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#define GS_EGL_X11 1
#define GS_EGL_WL 0

class GSWndRetroGL : public GSWndGL
{
	void PopulateWndGlFunction();
	void CreateContext(int major, int minor);
	void BindAPI();
	void SetSwapInterval() final;
	bool HasLateVsyncSupport() final { return false; }
	void OpenEGLDisplay();
	void CloseEGLDisplay();

public:
	GSWndRetroGL();
	virtual ~GSWndRetroGL() {}

	bool Create(const std::string& title, int w, int h) final;
	bool Attach(void* handle, bool managed = true) final;
	void Detach() final;

	virtual void* CreateNativeDisplay();
	virtual void* CreateNativeWindow(int w, int h); // GSopen1/PSX API
	virtual void* AttachNativeWindow(void* handle);
	virtual void DestroyNativeResources();

	GSVector4i GetClientRect();
	virtual bool SetWindowText(const char* title); // GSopen1/PSX API

	void AttachContext() final;
	void DetachContext() final;
	void* GetProcAddress(const char* name, bool opt = false) final;

	void Flip() final;

	// Deprecated API
	void Show() final {}
	void Hide() final {}
	void HideFrame() final {} // DX9 API

	void* GetDisplay() final { return (void*)-1; } // GSopen1 API
	void* GetHandle() final { return (void*)-1; }  // DX API


	// Static to allow to query supported the platform
	// before object creation
	static int SelectPlatform();
};

#endif
