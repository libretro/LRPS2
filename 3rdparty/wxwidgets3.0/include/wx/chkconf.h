/*
 * Name:        wx/chkconf.h
 * Purpose:     check the config settings for consistency
 * Author:      Vadim Zeitlin
 * Modified by:
 * Created:     09.08.00
 * Copyright:   (c) 2000 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */
#ifndef _WX_CHKCONF_H_
#define _WX_CHKCONF_H_

/*
              **************************************************
              PLEASE READ THIS IF YOU GET AN ERROR IN THIS FILE!
              **************************************************

    If you get an error saying "wxUSE_FOO must be defined", it means that you
    are not using the correct up-to-date version of setup.h. This happens most
    often when using svn or daily snapshots and a new symbol was added to
    setup0.h and you haven't updated your local setup.h to reflect it. If
    this is the case, you need to propagate the changes from setup0.h to your
    setup.h and, if using makefiles under MSW, also remove setup.h under the
    build directory (lib/$(COMPILER)_{lib,dll}/msw[u][d][dll]/wx) so that
    the new setup.h is copied there.

    If you get an error of the form "wxFoo requires wxBar", then the settings
    in your setup.h are inconsistent. You have the choice between correcting
    them manually or commenting out #define wxABORT_ON_CONFIG_ERROR below to
    try to correct the problems automatically (not really recommended but
    might work).
 */

/*
   This file has the following sections:
    1. checks that all wxUSE_XXX symbols we use are defined
     a) first the non-GUI ones
     b) then the GUI-only ones
    2. platform-specific checks done in the platform headers
    3. generic consistency checks
     a) first the non-GUI ones
     b) then the GUI-only ones
 */

/*
   global features
 */

/*
    If we're compiling without support for threads/exceptions we have to
    disable the corresponding features.
 */
#ifdef wxNO_THREADS
#   undef wxUSE_THREADS
#   define wxUSE_THREADS 0
#endif /* wxNO_THREADS */

/*
   Section 1a: tests for non GUI features.

   please keep the options in alphabetical order!
 */

#ifndef wxUSE_COMPILER_TLS
#define wxUSE_COMPILER_TLS 0
#endif /* !defined(wxUSE_COMPILER_TLS) */

#ifndef wxUSE_CONSOLE_EVENTLOOP
#define wxUSE_CONSOLE_EVENTLOOP 0
#endif /* !defined(wxUSE_CONSOLE_EVENTLOOP) */

#ifndef wxUSE_IPV6
#       define wxUSE_IPV6 0
#endif /* !defined(wxUSE_IPV6) */

#ifndef wxUSE_LONGLONG
#       define wxUSE_LONGLONG 0
#endif /* !defined(wxUSE_LONGLONG) */

#ifndef wxUSE_PRINTF_POS_PARAMS
#       define wxUSE_PRINTF_POS_PARAMS 0
#endif /* !defined(wxUSE_PRINTF_POS_PARAMS) */

#ifndef wxUSE_PROTOCOL
#       define wxUSE_PROTOCOL 0
#endif /* !defined(wxUSE_PROTOCOL) */

/* we may not define wxUSE_PROTOCOL_XXX if wxUSE_PROTOCOL is set to 0 */
#if !wxUSE_PROTOCOL
#   undef wxUSE_PROTOCOL_HTTP
#   undef wxUSE_PROTOCOL_FTP
#   undef wxUSE_PROTOCOL_FILE
#   define wxUSE_PROTOCOL_HTTP 0
#   define wxUSE_PROTOCOL_FTP 0
#   define wxUSE_PROTOCOL_FILE 0
#endif /* wxUSE_PROTOCOL */

#ifndef wxUSE_PROTOCOL_HTTP
#       define wxUSE_PROTOCOL_HTTP 0
#endif /* !defined(wxUSE_PROTOCOL_HTTP) */

#ifndef wxUSE_PROTOCOL_FTP
#       define wxUSE_PROTOCOL_FTP 0
#endif /* !defined(wxUSE_PROTOCOL_FTP) */

#ifndef wxUSE_PROTOCOL_FILE
#       define wxUSE_PROTOCOL_FILE 0
#endif /* !defined(wxUSE_PROTOCOL_FILE) */

#ifndef wxUSE_STD_STRING_CONV_IN_WXSTRING
#       define wxUSE_STD_STRING_CONV_IN_WXSTRING 0
#endif /* !defined(wxUSE_STD_STRING_CONV_IN_WXSTRING) */

#ifndef wxUSE_STREAMS
#       define wxUSE_STREAMS 0
#endif /* !defined(wxUSE_STREAMS) */

#ifndef wxUSE_TEXTBUFFER
#       define wxUSE_TEXTBUFFER 0
#endif /* !defined(wxUSE_TEXTBUFFER) */

#ifndef wxUSE_TEXTFILE
#       define wxUSE_TEXTFILE 0
#endif /* !defined(wxUSE_TEXTFILE) */

#ifndef wxUSE_UNICODE
#       define wxUSE_UNICODE 0
#endif /* !defined(wxUSE_UNICODE) */

/*
   Section 2: platform-specific checks.

   This must be done after checking that everything is defined as the platform
   checks use wxUSE_XXX symbols in #if tests.
 */

#if defined(__WINDOWS__)
#  include "wx/msw/chkconf.h"
#elif defined(__WXMAC__)
#  include "wx/osx/chkconf.h"
#elif defined(__WXANDROID__)
#  include "wx/android/chkconf.h"
#endif

/*
    __UNIX__ is also defined under Cygwin but we shouldn't perform these checks
    there if we're building Windows ports.
 */
#if defined(__UNIX__) && !defined(__WINDOWS__)
#   include "wx/unix/chkconf.h"
#endif

/*
   Section 3a: check consistency of the non-GUI settings.
 */

#if wxUSE_PROTOCOL_FILE || wxUSE_PROTOCOL_FTP || wxUSE_PROTOCOL_HTTP
#   if !wxUSE_PROTOCOL
#            undef wxUSE_PROTOCOL
#            define wxUSE_PROTOCOL 1
#   endif
#endif /* wxUSE_PROTOCOL_XXX */

#if wxUSE_PROTOCOL
#   if !wxUSE_STREAMS
#           undef wxUSE_STREAMS
#           define wxUSE_STREAMS 1
#   endif
#endif /* wxUSE_PROTOCOL */

#if wxUSE_DATETIME
#    if !wxUSE_LONGLONG
#            undef wxUSE_LONGLONG
#            define wxUSE_LONGLONG 1
#    endif
#endif

#if wxUSE_TEXTFILE && !wxUSE_TEXTBUFFER
#       undef wxUSE_TEXTBUFFER
#       define wxUSE_TEXTBUFFER 1
#endif /* wxUSE_TEXTFILE */

#if wxUSE_TEXTFILE && !wxUSE_FILE
#       undef wxUSE_FILE
#       define wxUSE_FILE 1
#endif /* wxUSE_TEXTFILE */

#endif /* _WX_CHKCONF_H_ */
