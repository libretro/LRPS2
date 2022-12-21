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
#include "SafeArray.h"
#include "ScopedAlloc.h"

#include <wx/tokenzr.h>

#define WX_STR(str) (static_cast<const char *>(str.c_str()))

// --------------------------------------------------------------------------------------
//  pxToUTF8
// --------------------------------------------------------------------------------------
// Converts a string to UTF8 and provides an interface for getting its length.
class pxToUTF8
{
    DeclareNoncopyableObject(pxToUTF8);

protected:
    wxCharBuffer m_result;
    int m_length;

public:
    explicit pxToUTF8(const wxString &src)
        : m_result(src.ToUTF8())
    {
        m_length = -1;
    }

    size_t Length()
    {
        if (m_length == -1)
            m_length = strlen(m_result);
        return m_length;
    }

    operator const char *() const
    {
        return m_result.data();
    }
};

// --------------------------------------------------------------------------------------
//  ParsedAssignmentString
// --------------------------------------------------------------------------------------
// This class is a simple helper for parsing INI-style assignments, in the typical form of:
//    variable = value
//    filename = SomeString.txt
//    integer  = 15
//
// This parser supports both '//' and ';' at the head of a line as indicators of a commented
// line, and such a line will return empty strings for l- and r-value.
//
// No type handling is performed -- the user must manually parse strings into integers, etc.
// For advanced "fully functional" ini file parsing, consider using wxFileConfig instead.
//
struct ParsedAssignmentString
{
    wxString lvalue;
    wxString rvalue;
    bool IsComment;

    ParsedAssignmentString(const wxString &src);
};

// ======================================================================================
//  FastFormatAscii / FastFormatUnicode  (overview!)
// ======================================================================================
// Fast formatting of ASCII or Unicode text.  These classes uses a series of thread-local
// format buffers that are allocated only once and grown to accommodate string formatting
// needs.  Because the buffers are thread-local, no thread synch objects are required in
// order to format strings, allowing for multi-threaded string formatting operations to be
// performed with maximum efficiency.  This class also reduces the overhead typically required
// to allocate string buffers off the heap.
//
// Drawbacks:
//  * Some overhead is added to the creation and destruction of threads, however since thread
//    construction is a typically slow process, and often avoided to begin with, this should
//    be a sound trade-off.
//
// Notes:
//  * conversion to wxString requires a heap allocation.
//  * FastFormatUnicode can accept either UTF8 or UTF16/32 (wchar_t) input, but FastFormatAscii
//    accepts Ascii/UTF8 only.
//

typedef ScopedAlignedAlloc<char, 16> CharBufferType;
// --------------------------------------------------------------------------------------
//  FastFormatAscii
// --------------------------------------------------------------------------------------

class FastFormatAscii
{
protected:
    CharBufferType m_dest;

public:
    FastFormatAscii();
    ~FastFormatAscii() = default;
    FastFormatAscii &Write(const char *fmt, ...);
    FastFormatAscii &WriteV(const char *fmt, va_list argptr);

    void Clear();
    bool IsEmpty() const;

    const char *c_str() const { return m_dest.GetPtr(); }
    operator const char *() const { return m_dest.GetPtr(); }

    FastFormatAscii &operator+=(const wxString &s)
    {
        Write("%s", WX_STR(s));
        return *this;
    }

    FastFormatAscii &operator+=(const wxChar *psz)
    {
        Write("%ls", psz);
        return *this;
    }

    FastFormatAscii &operator+=(const char *psz)
    {
        Write("%s", psz);
        return *this;
    }
};

// --------------------------------------------------------------------------------------
//  FastFormatUnicode
// --------------------------------------------------------------------------------------
class FastFormatUnicode
{
protected:
    CharBufferType m_dest;
    uint m_Length;

public:
    FastFormatUnicode();
    ~FastFormatUnicode() = default;

    FastFormatUnicode &Write(const char *fmt, ...);
    FastFormatUnicode &Write(const wxChar *fmt, ...);
    FastFormatUnicode &Write(const wxString fmt, ...);
    FastFormatUnicode &WriteV(const char *fmt, va_list argptr);
    FastFormatUnicode &WriteV(const wxChar *fmt, va_list argptr);

    void Clear();
    bool IsEmpty() const;
    uint Length() const { return m_Length; }

    const wxChar *c_str() const { return (const wxChar *)m_dest.GetPtr(); }
    operator const wxChar *() const { return (const wxChar *)m_dest.GetPtr(); }
    operator wxString() const { return (const wxChar *)m_dest.GetPtr(); }

    FastFormatUnicode &operator+=(const wxString &s)
    {
        Write(L"%s", WX_STR(s));
        return *this;
    }

    FastFormatUnicode &operator+=(const wxChar *psz)
    {
        Write(L"%s", psz);
        return *this;
    }

    FastFormatUnicode &operator+=(const char *psz);
};

#define pxsFmt FastFormatUnicode().Write

extern wxString &operator+=(wxString &str1, const FastFormatUnicode &str2);
extern wxString operator+(const wxString &str1, const FastFormatUnicode &str2);
extern wxString operator+(const wxChar *str1, const FastFormatUnicode &str2);
extern wxString operator+(const FastFormatUnicode &str1, const wxString &str2);
extern wxString operator+(const FastFormatUnicode &str1, const wxChar *str2);
