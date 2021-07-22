/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PS2EEXT_H_INCLUDED
#define PS2EEXT_H_INCLUDED

#include <stdio.h>
#include <string>
#include <cstdarg>

#if defined(_MSC_VER)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#undef Yield

#define EXPORT_C_(type) extern "C" type CALLBACK

#elif defined(__unix__)

#include <cstring>
#include <wx/defs.h>
#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type

#else

#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type

#endif

#include <PluginCompatibility.h>
//#include "PS2Edefs.h"

#if !defined(_MSC_VER) || !defined(UNICODE)
static void __forceinline PluginNullConfigure(std::string desc, s32 &log);
#else
static void __forceinline PluginNullConfigure(std::wstring desc, s32 &log);
#endif

enum FileMode {
    READ_FILE = 0,
    WRITE_FILE
};

static void __forceinline PluginNullConfigure(std::string desc, int &log)
{
}
#if defined(__unix__)
#define ENTRY_POINT /* We don't need no stinkin' entry point! */
#elif defined(__WXMAC__) || defined(__APPLE__)
#define ENTRY_POINT /* We don't need no stinkin' entry point! */ // TODO OSX WTF is this anyway?
#else
#define usleep(x) Sleep(x / 1000)
#define ENTRY_POINT                                     \
    HINSTANCE hInst;                                    \
                                                        \
    BOOL APIENTRY DllMain(HANDLE hModule, /* DLL INIT*/ \
                          DWORD dwReason,               \
                          LPVOID lpReserved)            \
    {                                                   \
        hInst = (HINSTANCE)hModule;                     \
        return TRUE; /* very quick :)*/                 \
    }
#endif

#endif // PS2EEXT_H_INCLUDED
