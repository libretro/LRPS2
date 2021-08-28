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
#include "wx/versioninfo.h"
#include "wx/meta/implicitconversion.h"

class WXDLLIMPEXP_FWD_BASE wxArrayString;
class WXDLLIMPEXP_FWD_BASE wxArrayInt;

// need this for wxGetDiskSpace() as we can't, unfortunately, forward declare
// wxLongLong
#include "wx/longlong.h"

// needed for wxOperatingSystemId, wxLinuxDistributionInfo
#include "wx/platinfo.h"

#ifdef __WATCOMC__
    #include <direct.h>
#elif defined(__X__)
    #include <dirent.h>
    #include <unistd.h>
#endif

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
// String functions (deprecated, use wxString)
// ----------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_8
// A shorter way of using strcmp
wxDEPRECATED_INLINE(inline bool wxStringEq(const char *s1, const char *s2),
    return wxCRT_StrcmpA(s1, s2) == 0; )

#if wxUSE_UNICODE
wxDEPRECATED_INLINE(inline bool wxStringEq(const wchar_t *s1, const wchar_t *s2),
    return wxCRT_StrcmpW(s1, s2) == 0; )
#endif // wxUSE_UNICODE

#endif // WXWIN_COMPATIBILITY_2_8

// ----------------------------------------------------------------------------
// Miscellaneous functions
// ----------------------------------------------------------------------------

// Get OS description as a user-readable string
WXDLLIMPEXP_BASE wxString wxGetOsDescription();

// Get OS version
WXDLLIMPEXP_BASE wxOperatingSystemId wxGetOsVersion(int *majorVsn = NULL,
                                                    int *minorVsn = NULL);

// Get platform endianness
WXDLLIMPEXP_BASE bool wxIsPlatformLittleEndian();

// Get platform architecture
WXDLLIMPEXP_BASE bool wxIsPlatform64Bit();

// ----------------------------------------------------------------------------
// wxPlatform
// ----------------------------------------------------------------------------

/*
 * Class to make it easier to specify platform-dependent values
 *
 * Examples:
 *  long val = wxPlatform::If(wxMac, 1).ElseIf(wxGTK, 2).ElseIf(stPDA, 5).Else(3);
 *  wxString strVal = wxPlatform::If(wxMac, wxT("Mac")).ElseIf(wxMSW, wxT("MSW")).Else(wxT("Other"));
 *
 * A custom platform symbol:
 *
 *  #define stPDA 100
 *  #ifdef __WXWINCE__
 *      wxPlatform::AddPlatform(stPDA);
 *  #endif
 *
 *  long windowStyle = wxCAPTION | (long) wxPlatform::IfNot(stPDA, wxRESIZE_BORDER);
 *
 */

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

    static wxPlatform If(int platform, long value);
    static wxPlatform IfNot(int platform, long value);
    wxPlatform& ElseIf(int platform, long value);
    wxPlatform& ElseIfNot(int platform, long value);
    wxPlatform& Else(long value);

    static wxPlatform If(int platform, int value) { return If(platform, (long)value); }
    static wxPlatform IfNot(int platform, int value) { return IfNot(platform, (long)value); }
    wxPlatform& ElseIf(int platform, int value) { return ElseIf(platform, (long) value); }
    wxPlatform& ElseIfNot(int platform, int value) { return ElseIfNot(platform, (long) value); }
    wxPlatform& Else(int value) { return Else((long) value); }

    static wxPlatform If(int platform, double value);
    static wxPlatform IfNot(int platform, double value);
    wxPlatform& ElseIf(int platform, double value);
    wxPlatform& ElseIfNot(int platform, double value);
    wxPlatform& Else(double value);

    static wxPlatform If(int platform, const wxString& value);
    static wxPlatform IfNot(int platform, const wxString& value);
    wxPlatform& ElseIf(int platform, const wxString& value);
    wxPlatform& ElseIfNot(int platform, const wxString& value);
    wxPlatform& Else(const wxString& value);

    long GetInteger() const { return m_longValue; }
    const wxString& GetString() const { return m_stringValue; }
    double GetDouble() const { return m_doubleValue; }

    operator int() const { return (int) GetInteger(); }
    operator long() const { return GetInteger(); }
    operator double() const { return GetDouble(); }
    operator const wxString&() const { return GetString(); }

    static void AddPlatform(int platform);
    static bool Is(int platform);
    static void ClearPlatforms();

private:

    void Init() { m_longValue = 0; m_doubleValue = 0.0; }

    long                m_longValue;
    double              m_doubleValue;
    wxString            m_stringValue;
    static wxArrayInt*  sm_customPlatforms;
};

/// Function for testing current platform
inline bool wxPlatformIs(int platform) { return wxPlatform::Is(platform); }

// ----------------------------------------------------------------------------
// Various conversions
// ----------------------------------------------------------------------------

// Convert 2-digit hex number to decimal
WXDLLIMPEXP_BASE int wxHexToDec(const wxString& buf);

// Convert 2-digit hex number to decimal
inline int wxHexToDec(const char* buf)
{
    int firstDigit, secondDigit;

    if (buf[0] >= 'A')
        firstDigit = buf[0] - 'A' + 10;
    else
        firstDigit = buf[0] - '0';

    if (buf[1] >= 'A')
        secondDigit = buf[1] - 'A' + 10;
    else
        secondDigit = buf[1] - '0';

    return (firstDigit & 0xF) * 16 + (secondDigit & 0xF );
}


// Convert decimal integer to 2-character hex string
WXDLLIMPEXP_BASE void wxDecToHex(int dec, wxChar *buf);
WXDLLIMPEXP_BASE void wxDecToHex(int dec, char* ch1, char* ch2);
WXDLLIMPEXP_BASE wxString wxDecToHex(int dec);

// ----------------------------------------------------------------------------
// Process management
// ----------------------------------------------------------------------------

// Sleep for nSecs seconds
WXDLLIMPEXP_BASE void wxSleep(int nSecs);

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


#if wxUSE_LONGLONG
    typedef wxLongLong wxDiskspaceSize_t;
#else
    typedef long wxDiskspaceSize_t;
#endif

#endif
    // _WX_UTILSH__
