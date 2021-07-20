///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/wince/chkconf.h
// Purpose:     WinCE-specific configuration options checks
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2005-03-07
// Copyright:   (c) 2005 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_WINCE_CHKCONF_H_
#define _WX_MSW_WINCE_CHKCONF_H_

// ----------------------------------------------------------------------------
// Disable features which don't work or don't make sense under CE
// ----------------------------------------------------------------------------

// please keep the list in alphabetic order except for closely related settings
// (e.g. wxUSE_ENH_METAFILE is put immediately after wxUSE_METAFILE)

// eVC doesn't have standard streams
#ifdef __EVC4__
    #undef wxUSE_STD_IOSTREAM
    #define wxUSE_STD_IOSTREAM 0
#endif

// libtiff doesn't build with eVC but is ok with VC8
#ifdef __EVC4__
    #undef wxUSE_LIBTIFF
    #define wxUSE_LIBTIFF 0
#endif

// eVC doesn't support SEH
#undef wxUSE_ON_FATAL_EXCEPTION
#define wxUSE_ON_FATAL_EXCEPTION 0

// Standard SDK lacks a few things, forcefully disable them
#ifdef WCE_PLATFORM_STANDARDSDK
    // no shell functions support
    #undef wxUSE_STDPATHS
    #define wxUSE_STDPATHS 0
#endif // WCE_PLATFORM_STANDARDSDK

#undef wxUSE_UNICODE_MSLU
#define wxUSE_UNICODE_MSLU 0

#undef wxUSE_WXHTML_HELP
#define wxUSE_WXHTML_HELP 0

#endif // _WX_MSW_WINCE_CHKCONF_H_

