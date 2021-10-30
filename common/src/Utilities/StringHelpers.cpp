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

#include "PrecompiledHeader.h"
#include <wx/gdicmn.h> // for wxPoint/wxRect stuff

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

__fi wxString fromAscii(const char *src)
{
    return wxString::FromAscii(src);
}

wxString u128::ToString() const
{
    return pxsFmt(L"0x%08X.%08X.%08X.%08X", _u32[0], _u32[1], _u32[2], _u32[3]);
}

// Splits a string into parts and adds the parts into the given SafeList.
// This list is not cleared, so concatenating many splits into a single large list is
// the 'default' behavior, unless you manually clear the SafeList prior to subsequent calls.
//
// Note: wxWidgets 2.9 / 3.0 has a wxSplit function, but we're using 2.8 so I had to make
// my own.
void SplitString(wxArrayString &dest, const wxString &src, const wxString &delims, wxStringTokenizerMode mode)
{
    wxStringTokenizer parts(src, delims, mode);
    while (parts.HasMoreTokens())
        dest.Add(parts.GetNextToken());
}


// --------------------------------------------------------------------------------------
//  ToString helpers for wxString!
// --------------------------------------------------------------------------------------

// Converts a wxPoint into a comma-delimited string!
wxString ToString(const wxPoint &src, const wxString &separator)
{
    return wxString() << src.x << separator << src.y;
}

wxString ToString(const wxSize &src, const wxString &separator)
{
    return wxString() << src.GetWidth() << separator << src.GetHeight();
}

// Converts a wxRect into a comma-delimited string!
// Example: 32,64,128,5
wxString ToString(const wxRect &src, const wxString &separator)
{
    return ToString(src.GetLeftTop(), separator) << separator << ToString(src.GetSize(), separator);
}

// --------------------------------------------------------------------------------------
//  Parse helpers for wxString!
// --------------------------------------------------------------------------------------

// returns TRUE if the parse is valid, or FALSE if it's a comment.
bool pxParseAssignmentString(const wxString &src, wxString &ldest, wxString &rdest)
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
