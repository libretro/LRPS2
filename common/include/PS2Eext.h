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
#undef Yield

#define EXPORT_C_(type) extern "C" type CALLBACK

#elif defined(__unix__)

#include <cstring>
#include <wx/defs.h>
#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type

#else

#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type

#endif

#ifdef _WIN32
#include <windows.h>
#if _MSC_VER
#include <windowsx.h>
#include <commctrl.h>
#endif
#endif

#include <vector>
#include <cstdio>

#endif // PS2EEXT_H_INCLUDED
