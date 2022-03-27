/*
 * Name:        wx/dlimpexp.h
 * Purpose:     Macros for declaring DLL-imported/exported functions
 * Author:      Vadim Zeitlin
 * Modified by:
 * Created:     16.10.2003 (extracted from wx/defs.h)
 * Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

/*
    This is a C file, not C++ one, do not use C++ comments here!
 */

#ifndef _WX_DLIMPEXP_H_
#define _WX_DLIMPEXP_H_

#if defined(HAVE_VISIBILITY)
#    define WXEXPORT __attribute__ ((visibility("default")))
#    define WXIMPORT __attribute__ ((visibility("default")))
#elif defined(__WINDOWS__)
    /*
       __declspec works in BC++ 5 and later, Watcom C++ 11.0 and later as well
       as VC++.
     */
#    if defined(__VISUALC__)
#        define WXEXPORT __declspec(dllexport)
#        define WXIMPORT __declspec(dllimport)
    /*
        While gcc also supports __declspec(dllexport), it creates unusably huge
        DLL files since gcc 4.5 (while taking horribly long amounts of time),
        see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43601. Because of this
        we rely on binutils auto export/import support which seems to work
        quite well for 4.5+.
     */
#    elif defined(__GNUC__) && !wxCHECK_GCC_VERSION(4, 5)
        /*
            __declspec could be used here too but let's use the native
            __attribute__ instead for clarity.
        */
#       define WXEXPORT __attribute__((dllexport))
#       define WXIMPORT __attribute__((dllimport))
#    endif
#elif defined(__CYGWIN__)
#    define WXEXPORT __declspec(dllexport)
#    define WXIMPORT __declspec(dllimport)
#endif

/* for other platforms/compilers we don't anything */
#ifndef WXEXPORT
#    define WXEXPORT
#    define WXIMPORT
#endif

#    define WXDLLIMPEXP_BASE
#    define WXDLLIMPEXP_DATA_BASE(type) type
#    define WXDLLIMPEXP_INLINE_BASE

#    define WXDLLIMPEXP_NET
#    define WXDLLIMPEXP_DATA_NET(type) type

#    define WXDLLIMPEXP_CORE
#    define WXDLLIMPEXP_DATA_CORE(type) type
#    define WXDLLIMPEXP_INLINE_CORE

#    define WXDLLIMPEXP_ADV
#    define WXDLLIMPEXP_DATA_ADV(type) type

#    define WXDLLIMPEXP_QA
#    define WXDLLIMPEXP_DATA_QA(type) type

#    define WXDLLIMPEXP_HTML
#    define WXDLLIMPEXP_DATA_HTML(type) type

#    define WXDLLIMPEXP_GL

#    define WXDLLIMPEXP_XML

#    define WXDLLIMPEXP_XRC

#    define WXDLLIMPEXP_AUI

#    define WXDLLIMPEXP_PROPGRID
#    define WXDLLIMPEXP_DATA_PROPGRID(type) type

#    define WXDLLIMPEXP_RIBBON

#    define WXDLLIMPEXP_RICHTEXT

#    define WXDLLIMPEXP_MEDIA

#    define WXDLLIMPEXP_STC
#    define WXDLLIMPEXP_DATA_STC(type) type

#    define WXDLLIMPEXP_WEBVIEW
#    define WXDLLIMPEXP_DATA_WEBVIEW(type) type

/*
   GCC warns about using __attribute__ (and also __declspec in mingw32 case) on
   forward declarations while MSVC complains about forward declarations without
   __declspec for the classes later declared with it, so we need a separate set
   of macros for forward declarations to hide this difference:
 */
#if defined(HAVE_VISIBILITY) || (defined(__WINDOWS__) && defined(__GNUC__))
    #define WXDLLIMPEXP_FWD_BASE
    #define WXDLLIMPEXP_FWD_NET
    #define WXDLLIMPEXP_FWD_CORE
    #define WXDLLIMPEXP_FWD_ADV
    #define WXDLLIMPEXP_FWD_QA
    #define WXDLLIMPEXP_FWD_HTML
    #define WXDLLIMPEXP_FWD_GL
    #define WXDLLIMPEXP_FWD_XML
    #define WXDLLIMPEXP_FWD_XRC
    #define WXDLLIMPEXP_FWD_AUI
    #define WXDLLIMPEXP_FWD_PROPGRID
    #define WXDLLIMPEXP_FWD_RIBBON
    #define WXDLLIMPEXP_FWD_RICHTEXT
    #define WXDLLIMPEXP_FWD_MEDIA
    #define WXDLLIMPEXP_FWD_STC
    #define WXDLLIMPEXP_FWD_WEBVIEW
#else
    #define WXDLLIMPEXP_FWD_BASE      WXDLLIMPEXP_BASE
    #define WXDLLIMPEXP_FWD_NET       WXDLLIMPEXP_NET
    #define WXDLLIMPEXP_FWD_CORE      WXDLLIMPEXP_CORE
    #define WXDLLIMPEXP_FWD_ADV       WXDLLIMPEXP_ADV
    #define WXDLLIMPEXP_FWD_QA        WXDLLIMPEXP_QA
    #define WXDLLIMPEXP_FWD_HTML      WXDLLIMPEXP_HTML
    #define WXDLLIMPEXP_FWD_GL        WXDLLIMPEXP_GL
    #define WXDLLIMPEXP_FWD_XML       WXDLLIMPEXP_XML
    #define WXDLLIMPEXP_FWD_XRC       WXDLLIMPEXP_XRC
    #define WXDLLIMPEXP_FWD_AUI       WXDLLIMPEXP_AUI
    #define WXDLLIMPEXP_FWD_PROPGRID  WXDLLIMPEXP_PROPGRID
    #define WXDLLIMPEXP_FWD_RIBBON    WXDLLIMPEXP_RIBBON
    #define WXDLLIMPEXP_FWD_RICHTEXT  WXDLLIMPEXP_RICHTEXT
    #define WXDLLIMPEXP_FWD_MEDIA     WXDLLIMPEXP_MEDIA
    #define WXDLLIMPEXP_FWD_STC       WXDLLIMPEXP_STC
    #define WXDLLIMPEXP_FWD_WEBVIEW   WXDLLIMPEXP_WEBVIEW
#endif

/* for backwards compatibility, define suffix-less versions too */
#define WXDLLEXPORT WXDLLIMPEXP_CORE
#define WXDLLEXPORT_DATA WXDLLIMPEXP_DATA_CORE

/*
   MSVC up to 6.0 needs to be explicitly told to export template instantiations
   used by the DLL clients, use this macro to do it like this:

       template <typename T> class Foo { ... };
       WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE( Foo<int> )

   (notice that currently we only need this for wxBase and wxCore libraries)
 */
#if defined(__VISUALC__) && (__VISUALC__ <= 1200)
        /*
           We need to disable this warning when using this macro, as
           recommended by Microsoft itself:

           http://support.microsoft.com/default.aspx?scid=kb%3ben-us%3b168958
         */
        #pragma warning(disable:4231)

        #define WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE(decl) \
            extern template class WXDLLIMPEXP_BASE decl;
        #define WXDLLIMPEXP_TEMPLATE_INSTANCE_CORE(decl) \
            extern template class WXDLLIMPEXP_CORE decl;
#else /* not VC <= 6 */
    #define WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE(decl)
    #define WXDLLIMPEXP_TEMPLATE_INSTANCE_CORE(decl)
#endif /* VC6/others */

#endif /* _WX_DLIMPEXP_H_ */

