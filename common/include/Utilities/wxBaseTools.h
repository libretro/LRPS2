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

#pragma once

#include "Dependencies.h"

// --------------------------------------------------------------------------------------
//  wxDoNotLogInThisScope
// --------------------------------------------------------------------------------------
// This class is used to disable wx's sometimes inappropriate amount of forced error logging
// during specific activities.  For example, when using wxDynamicLibrary to detect the
// validity of DLLs, wx will log errors for missing symbols. (sigh)
//
// Usage: Basic auto-cleanup destructor class.  Create an instance inside a scope, and
// logging will be re-enabled when scope is terminated. :)
//
class wxDoNotLogInThisScope
{
    DeclareNoncopyableObject(wxDoNotLogInThisScope);

#ifndef __LIBRETRO__
protected:
    bool m_prev;
#endif

public:
    wxDoNotLogInThisScope()
    {
#ifndef __LIBRETRO__
        m_prev = wxLog::EnableLogging(false);
#endif
    }

    virtual ~wxDoNotLogInThisScope()
    {
#ifndef __LIBRETRO__
        wxLog::EnableLogging(m_prev);
#endif
    }
};


extern wxString pxReadLine(wxInputStream &input);
extern void pxReadLine(wxInputStream &input, wxString &dest);
extern void pxReadLine(wxInputStream &input, wxString &dest, std::string &intermed);
extern bool pxReadLine(wxInputStream &input, std::string &dest);
extern void pxWriteLine(wxOutputStream &output);
extern void pxWriteLine(wxOutputStream &output, const wxString &text);
extern void pxWriteMultiline(wxOutputStream &output, const wxString &src);
