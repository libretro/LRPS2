/*
 * Name:        wx/msw/chkconf.h
 * Purpose:     Compiler-specific configuration checking
 * Author:      Julian Smart
 * Modified by:
 * Created:     01/02/97
 * Copyright:   (c) Julian Smart
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#ifndef _WX_MSW_CHKCONF_H_
#define _WX_MSW_CHKCONF_H_

#ifndef wxUSE_DC_CACHEING
#       define wxUSE_DC_CACHEING 1
#endif /* wxUSE_DC_CACHEING */

#ifndef wxUSE_MS_HTML_HELP
#        define wxUSE_MS_HTML_HELP 0
#endif /* !defined(wxUSE_MS_HTML_HELP) */

#ifndef wxUSE_UNICODE_MSLU
#        define wxUSE_UNICODE_MSLU 0
#endif  /* wxUSE_UNICODE_MSLU */

/*
 * We don't want to give an error if wxUSE_UNICODE_MSLU is enabled but
 * wxUSE_UNICODE is not as this would make it impossible to simply set the
 * former in wx/setup.h as then the library wouldn't compile in non-Unicode
 * configurations, so instead simply unset it silently when it doesn't make
 * sense.
 */
#if wxUSE_UNICODE_MSLU && !wxUSE_UNICODE
#   undef wxUSE_UNICODE_MSLU
#   define wxUSE_UNICODE_MSLU 0
#endif


/*
 * Unfortunately we can't use compiler TLS support if the library can be used
 * inside a dynamically loaded DLL under Windows XP, as this can result in hard
 * to diagnose crashes due to the bugs in Windows TLS support, see #13116.
 *
 * So we disable it unless we can be certain that the code will never run under
 * XP, as is the case if we're using a compiler which doesn't support XP such
 * as MSVC 11+, unless it's used with the special "_xp" toolset, in which case
 * _USING_V110_SDK71_ is defined.
 *
 * However defining wxUSE_COMPILER_TLS as 2 overrides this safety check, see
 * the comments in wx/setup.h.
 */
#if wxUSE_COMPILER_TLS == 1
    #if !wxCHECK_VISUALC_VERSION(11) || defined(_USING_V110_SDK71_)
        #undef wxUSE_COMPILER_TLS
        #define wxUSE_COMPILER_TLS 0
    #endif
#endif


/*
 * disable the settings which don't work for some compilers
 */

#ifndef wxUSE_NORLANDER_HEADERS
#   if ( wxCHECK_WATCOM_VERSION(1,0) || defined(__WINE__) ) || \
       ((defined(__MINGW32__) || defined(__CYGWIN__)) && ((__GNUC__>2) ||((__GNUC__==2) && (__GNUC_MINOR__>=95))))
#       define wxUSE_NORLANDER_HEADERS 1
#   else
#       define wxUSE_NORLANDER_HEADERS 0
#   endif
#endif

/*
 * All of the settings below require SEH support (__try/__catch) and can't work
 * without it.
 */
#if !defined(_MSC_VER) && \
    (!defined(__BORLANDC__) || __BORLANDC__ < 0x0550)
#    undef wxUSE_ON_FATAL_EXCEPTION
#    define wxUSE_ON_FATAL_EXCEPTION 0
#endif /* compiler doesn't support SEH */

#if defined(__GNUWIN32__)
    /* These don't work as expected for mingw32 and cygwin32 */
/* some Cygwin versions don't have wcslen */
#   if defined(__CYGWIN__) || defined(__CYGWIN32__)
#   if ! ((__GNUC__>2) ||((__GNUC__==2) && (__GNUC_MINOR__>=95)))
#       undef wxUSE_WCHAR_T
#       define wxUSE_WCHAR_T 0
#   endif
#endif

#endif /* __GNUWIN32__ */

/* wxUSE_MFC is not defined when using configure as it doesn't make sense for
   gcc or mingw32 anyhow */
#ifndef wxUSE_MFC
    #define wxUSE_MFC 0
#endif /* !defined(wxUSE_MFC) */

#if wxUSE_SPINCTRL
#   if !wxUSE_SPINBTN
#           undef wxUSE_SPINBTN
#           define wxUSE_SPINBTN 1
#   endif
#endif

/*
   Compiler-specific checks.
 */

/* Borland */
#ifdef __BORLANDC__

#if __BORLANDC__ < 0x500
    /* BC++ 4.0 can't compile JPEG library */
#   undef wxUSE_LIBJPEG
#   define wxUSE_LIBJPEG 0
#endif

#endif /* __BORLANDC__ */

/*
   un/redefine the options which we can't compile (after checking that they're
   defined
 */
#ifdef __WINE__
#   if wxUSE_UNICODE_MSLU
#       undef wxUSE_UNICODE_MSLU
#       define wxUSE_UNICODE_MSLU 0
#   endif /* wxUSE_UNICODE_MSLU */
#endif /* __WINE__ */


#if !wxUSE_DYNAMIC_LOADER
#    if wxUSE_MS_HTML_HELP
#            undef wxUSE_MS_HTML_HELP
#            define wxUSE_MS_HTML_HELP 0
#    endif
#endif  /* !wxUSE_DYNAMIC_LOADER */

#endif /* _WX_MSW_CHKCONF_H_ */
