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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !(defined(_MSC_VER) && (_MSC_VER > 800))
#include <errno.h>
#endif

#include <time.h>

#ifdef __WXMAC__
    #include "wx/osx/private.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#if defined(__WINDOWS__)
    #include "wx/msw/private.h"
#endif

#if wxUSE_BASE

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
