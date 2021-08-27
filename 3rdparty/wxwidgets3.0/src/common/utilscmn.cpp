/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/utilscmn.cpp
// Purpose:     Miscellaneous utility functions and classes
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// See comment about this hack in time.cpp: here we do it for environ external
// variable which can't be easily declared when using MinGW in strict ANSI mode.
#ifdef wxNEEDS_STRICT_ANSI_WORKAROUNDS
    #undef __STRICT_ANSI__
    #include <stdlib.h>
    #define __STRICT_ANSI__
#endif

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/string.h"
    #include "wx/utils.h"
#endif // WX_PRECOMP

#include "wx/apptrait.h"

#include "wx/txtstrm.h"
#include "wx/config.h"
#include "wx/versioninfo.h"

#if defined(__WXWINCE__) && wxUSE_DATETIME
    #include "wx/datetime.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !wxONLY_WATCOM_EARLIER_THAN(1,4)
    #if !(defined(_MSC_VER) && (_MSC_VER > 800))
        #include <errno.h>
    #endif
#endif

#ifndef __WXWINCE__
    #include <time.h>
#else
    #include "wx/msw/wince/time.h"
#endif

#ifdef __WXMAC__
    #include "wx/osx/private.h"
#endif

#if !defined(__WXWINCE__)
    #include <sys/types.h>
    #include <sys/stat.h>
#endif

#if defined(__WINDOWS__)
    #include "wx/msw/private.h"
#endif

#if wxUSE_BASE

// ============================================================================
// implementation
// ============================================================================

// Array used in DecToHex conversion routine.
static const wxChar hexArray[] = wxT("0123456789ABCDEF");

// Convert 2-digit hex number to decimal
int wxHexToDec(const wxString& str)
{
    char buf[2];
    buf[0] = str.GetChar(0);
    buf[1] = str.GetChar(1);
    return wxHexToDec((const char*) buf);
}

// Convert decimal integer to 2-character hex string
void wxDecToHex(int dec, wxChar *buf)
{
    int firstDigit = (int)(dec/16.0);
    int secondDigit = (int)(dec - (firstDigit*16.0));
    buf[0] = hexArray[firstDigit];
    buf[1] = hexArray[secondDigit];
    buf[2] = 0;
}

// Convert decimal integer to 2 characters
void wxDecToHex(int dec, char* ch1, char* ch2)
{
    int firstDigit = (int)(dec/16.0);
    int secondDigit = (int)(dec - (firstDigit*16.0));
    (*ch1) = (char) hexArray[firstDigit];
    (*ch2) = (char) hexArray[secondDigit];
}

// Convert decimal integer to 2-character hex string
wxString wxDecToHex(int dec)
{
    wxChar buf[3];
    wxDecToHex(dec, buf);
    return wxString(buf);
}

// ----------------------------------------------------------------------------
// misc functions
// ----------------------------------------------------------------------------
bool wxIsPlatformLittleEndian()
{
    // Are we little or big endian? This method is from Harbison & Steele.
    union
    {
        long l;
        char c[sizeof(long)];
    } u;
    u.l = 1;

    return u.c[0] == 1;
}


// ----------------------------------------------------------------------------
// wxPlatform
// ----------------------------------------------------------------------------

/*
 * Class to make it easier to specify platform-dependent values
 */

wxArrayInt*  wxPlatform::sm_customPlatforms = NULL;

void wxPlatform::Copy(const wxPlatform& platform)
{
    m_longValue = platform.m_longValue;
    m_doubleValue = platform.m_doubleValue;
    m_stringValue = platform.m_stringValue;
}

wxPlatform wxPlatform::If(int platform, long value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, long value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, long value)
{
    if (Is(platform))
        m_longValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, long value)
{
    if (!Is(platform))
        m_longValue = value;
    return *this;
}

wxPlatform wxPlatform::If(int platform, double value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, double value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, double value)
{
    if (Is(platform))
        m_doubleValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, double value)
{
    if (!Is(platform))
        m_doubleValue = value;
    return *this;
}

wxPlatform wxPlatform::If(int platform, const wxString& value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, const wxString& value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, const wxString& value)
{
    if (Is(platform))
        m_stringValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, const wxString& value)
{
    if (!Is(platform))
        m_stringValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(long value)
{
    m_longValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(double value)
{
    m_doubleValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(const wxString& value)
{
    m_stringValue = value;
    return *this;
}

void wxPlatform::AddPlatform(int platform)
{
    if (!sm_customPlatforms)
        sm_customPlatforms = new wxArrayInt;
    sm_customPlatforms->Add(platform);
}

void wxPlatform::ClearPlatforms()
{
    wxDELETE(sm_customPlatforms);
}

/// Function for testing current platform

bool wxPlatform::Is(int platform)
{
#ifdef __WINDOWS__
    if (platform == wxOS_WINDOWS)
        return true;
#endif
#ifdef __WXWINCE__
    if (platform == wxOS_WINDOWS_CE)
        return true;
#endif

#if 0

// FIXME: wxWinPocketPC and wxWinSmartPhone are unknown symbols

#if defined(__WXWINCE__) && defined(__POCKETPC__)
    if (platform == wxWinPocketPC)
        return true;
#endif
#if defined(__WXWINCE__) && defined(__SMARTPHONE__)
    if (platform == wxWinSmartPhone)
        return true;
#endif

#endif

#ifdef __WXGTK__
    if (platform == wxPORT_GTK)
        return true;
#endif
#ifdef __WXMAC__
    if (platform == wxPORT_MAC)
        return true;
#endif
#ifdef __WXX11__
    if (platform == wxPORT_X11)
        return true;
#endif
#ifdef __UNIX__
    if (platform == wxOS_UNIX)
        return true;
#endif
#ifdef __OS2__
    if (platform == wxOS_OS2)
        return true;
#endif
#ifdef __WXPM__
    if (platform == wxPORT_PM)
        return true;
#endif
#ifdef __WXCOCOA__
    if (platform == wxPORT_MAC)
        return true;
#endif

    if (sm_customPlatforms && sm_customPlatforms->Index(platform) != wxNOT_FOUND)
        return true;

    return false;
}

// ----------------------------------------------------------------------------
// user id functions
// ----------------------------------------------------------------------------

wxString wxGetUserId()
{
    static const int maxLoginLen = 256; // FIXME arbitrary number

    wxString buf;
    bool ok = wxGetUserId(wxStringBuffer(buf, maxLoginLen), maxLoginLen);

    if ( !ok )
        buf.Empty();

    return buf;
}

wxString wxGetHomeDir()
{
    wxString home;
    wxGetHomeDir(&home);

    return home;
}
#endif // wxUSE_BASE
