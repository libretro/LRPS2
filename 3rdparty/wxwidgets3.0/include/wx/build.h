///////////////////////////////////////////////////////////////////////////////
// Name:        wx/build.h
// Purpose:     Runtime build options checking
// Author:      Vadim Zeitlin, Vaclav Slavik
// Modified by:
// Created:     07.05.02
// Copyright:   (c) 2002 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BUILD_H_
#define _WX_BUILD_H_

#include "wx/version.h"

// NB: This file contains macros for checking binary compatibility of libraries
//     in multilib builds, plugins and user components.
//     The WX_BUILD_OPTIONS_SIGNATURE macro expands into string that should
//     uniquely identify binary compatible builds: i.e. if two builds of the
//     library are binary compatible, their signature string should be the
//     same; if two builds are binary incompatible, their signatures should
//     be different.
//
//     Therefore, wxUSE_XXX flags that affect binary compatibility (vtables,
//     function signatures) should be accounted for here. So should compilers
//     and compiler versions (but note that binary compatible compiler versions
//     such as gcc-2.95.2 and gcc-2.95.3 should have same signature!).

// ----------------------------------------------------------------------------
// WX_BUILD_OPTIONS_SIGNATURE
// ----------------------------------------------------------------------------

#define __WX_BO_STRINGIZE(x)   __WX_BO_STRINGIZE0(x)
#define __WX_BO_STRINGIZE0(x)  #x

#if (wxMINOR_VERSION % 2) == 0
    #define __WX_BO_VERSION(x,y,z) \
        __WX_BO_STRINGIZE(x) "." __WX_BO_STRINGIZE(y)
#else
    #define __WX_BO_VERSION(x,y,z) \
        __WX_BO_STRINGIZE(x) "." __WX_BO_STRINGIZE(y) "." __WX_BO_STRINGIZE(z)
#endif

#if wxUSE_UNICODE_UTF8
    #define __WX_BO_UNICODE "UTF-8"
#elif wxUSE_UNICODE_WCHAR
    #define __WX_BO_UNICODE "wchar_t"
#else
    #define __WX_BO_UNICODE "ANSI"
#endif

// GCC and Intel C++ share same C++ ABI (and possibly others in the future),
// check if compiler versions are compatible:
#if defined(__GXX_ABI_VERSION)
    #define __WX_BO_COMPILER \
            ",compiler with C++ ABI " __WX_BO_STRINGIZE(__GXX_ABI_VERSION)
#elif defined(__GNUG__)
    #define __WX_BO_COMPILER ",GCC " \
            __WX_BO_STRINGIZE(__GNUC__) "." __WX_BO_STRINGIZE(__GNUC_MINOR__)
#elif defined(__VISUALC__)
    #define __WX_BO_COMPILER ",Visual C++ " __WX_BO_STRINGIZE(_MSC_VER)
#else
    #define __WX_BO_COMPILER
#endif

#define __WX_BO_WXWIN_COMPAT_2_6
#define __WX_BO_WXWIN_COMPAT_2_8

// deriving wxWin containers from STL ones changes them completely:
#define __WX_BO_STL ",wx containers"

#endif // _WX_BUILD_H_
