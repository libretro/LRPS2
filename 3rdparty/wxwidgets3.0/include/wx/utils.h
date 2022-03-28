/////////////////////////////////////////////////////////////////////////////
// Name:        wx/utils.h
// Purpose:     Miscellaneous utilities
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UTILS_H_
#define _WX_UTILS_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/object.h"
#include "wx/list.h"
#include "wx/filefn.h"
#include "wx/hashmap.h"
#include "wx/meta/implicitconversion.h"

class WXDLLIMPEXP_FWD_BASE wxArrayString;
class WXDLLIMPEXP_FWD_BASE wxArrayInt;

// need this for wxGetDiskSpace() as we can't, unfortunately, forward declare
// wxLongLong
#include "wx/longlong.h"

#include <stdio.h>

// ----------------------------------------------------------------------------
// Forward declaration
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxEventLoop;

// ----------------------------------------------------------------------------
// Arithmetic functions
// ----------------------------------------------------------------------------

template<typename T1, typename T2>
inline typename wxImplicitConversionType<T1,T2>::value
wxMax(T1 a, T2 b)
{
    typedef typename wxImplicitConversionType<T1,T2>::value ResultType;

    // Cast both operands to the same type before comparing them to avoid
    // warnings about signed/unsigned comparisons from some compilers:
    return static_cast<ResultType>(a) > static_cast<ResultType>(b) ? a : b;
}

template<typename T1, typename T2>
inline typename wxImplicitConversionType<T1,T2>::value
wxMin(T1 a, T2 b)
{
    typedef typename wxImplicitConversionType<T1,T2>::value ResultType;

    return static_cast<ResultType>(a) < static_cast<ResultType>(b) ? a : b;
}

template<typename T1, typename T2, typename T3>
inline typename wxImplicitConversionType3<T1,T2,T3>::value
wxClip(T1 a, T2 b, T3 c)
{
    typedef typename wxImplicitConversionType3<T1,T2,T3>::value ResultType;

    if ( static_cast<ResultType>(a) < static_cast<ResultType>(b) )
        return b;

    if ( static_cast<ResultType>(a) > static_cast<ResultType>(c) )
        return c;

    return a;
}

// ----------------------------------------------------------------------------
// wxMemorySize
// ----------------------------------------------------------------------------

// wxGetFreeMemory can return huge amount of memory on 32-bit platforms as well
// so to always use long long for its result type on all platforms which
// support it
#if wxUSE_LONGLONG
    typedef wxLongLong wxMemorySize;
#else
    typedef long wxMemorySize;
#endif

// ----------------------------------------------------------------------------
// wxPlatform
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxPlatform
{
public:
    wxPlatform() { Init(); }
    wxPlatform(const wxPlatform& platform) { Copy(platform); }
    void operator = (const wxPlatform& platform) { if (&platform != this) Copy(platform); }
    void Copy(const wxPlatform& platform);

    // Specify an optional default value
    wxPlatform(int defValue) { Init(); m_longValue = (long)defValue; }
    wxPlatform(long defValue) { Init(); m_longValue = defValue; }
    wxPlatform(const wxString& defValue) { Init(); m_stringValue = defValue; }
    wxPlatform(double defValue) { Init(); m_doubleValue = defValue; }

    long GetInteger() const { return m_longValue; }
    const wxString& GetString() const { return m_stringValue; }
    double GetDouble() const { return m_doubleValue; }

    operator int() const { return (int) GetInteger(); }
    operator long() const { return GetInteger(); }
    operator double() const { return GetDouble(); }
    operator const wxString&() const { return GetString(); }

private:

    void Init() { m_longValue = 0; m_doubleValue = 0.0; }

    long                m_longValue;
    double              m_doubleValue;
    wxString            m_stringValue;
    static wxArrayInt*  sm_customPlatforms;
};

// ----------------------------------------------------------------------------
// Process management
// ----------------------------------------------------------------------------

// Sleep for a given amount of milliseconds
WXDLLIMPEXP_BASE void wxMilliSleep(unsigned long milliseconds);

// Sleep for a given amount of microseconds
WXDLLIMPEXP_BASE void wxMicroSleep(unsigned long microseconds);

// ----------------------------------------------------------------------------
// Environment variables
// ----------------------------------------------------------------------------

// returns true if variable exists (value may be NULL if you just want to check
// for this)
WXDLLIMPEXP_BASE bool wxGetEnv(const wxString& var, wxString *value);

// ----------------------------------------------------------------------------
// Network and username functions.
// ----------------------------------------------------------------------------

// Get user ID e.g. jacs (this is known as login name under Unix)
WXDLLIMPEXP_BASE bool wxGetUserId(wxChar *buf, int maxSize);
WXDLLIMPEXP_BASE wxString wxGetUserId();

// Get current Home dir and copy to dest (returns pstr->c_str())
WXDLLIMPEXP_BASE wxString wxGetHomeDir();
WXDLLIMPEXP_BASE const wxChar* wxGetHomeDir(wxString *pstr);

// Get the user's (by default use the current user name) home dir,
// return empty string on error
WXDLLIMPEXP_BASE wxString wxGetUserHome(const wxString& user = wxEmptyString);

#endif
    // _WX_UTILSH__
