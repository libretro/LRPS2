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

#include "Dependencies.h"

#include "General.h"

__fi wxString fromUTF8(const char *src)
{
    // IMPORTANT:  We cannot use wxString::FromUTF8 because it *stupidly* relies on a C++ global instance of
    // wxMBConvUTF8().  C++ initializes and destroys these globals at random, so any object constructor or
    // destructor that attempts to do logging may crash the app (either during startup or during exit) unless
    // we use a LOCAL instance of wxMBConvUTF8(). --air

    // Performance?  No worries.  wxMBConvUTF8() is virtually free.  Initializing a stack copy of the class
    // is just as efficient as passing a pointer to a pre-instanced global. (which makes me wonder wh wxWidgets
    // uses the stupid globals in the first place!)  --air

    return wxString(src, wxMBConvUTF8());
}

// --------------------------------------------------------------------------------------
//  Parse helpers for wxString!
// --------------------------------------------------------------------------------------

// returns TRUE if the parse is valid, or FALSE if it's a comment.
static bool pxParseAssignmentString(const wxString &src, wxString &ldest, wxString &rdest)
{
    if (src.StartsWith(L"--") || src.StartsWith(L"//") || src.StartsWith(L";"))
        return false;

    ldest = src.BeforeFirst(L'=').Trim(true).Trim(false);
    rdest = src.AfterFirst(L'=').Trim(true).Trim(false);

    return true;
}

ParsedAssignmentString::ParsedAssignmentString(const wxString &src)
{
    IsComment = pxParseAssignmentString(src, lvalue, rvalue);
}
