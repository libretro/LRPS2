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
#   if \
       ((defined(__MINGW32__) || defined(__CYGWIN__)) && ((__GNUC__>2) ||((__GNUC__==2) && (__GNUC_MINOR__>=95))))
#       define wxUSE_NORLANDER_HEADERS 1
#   else
#       define wxUSE_NORLANDER_HEADERS 0
#   endif
#endif

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

#endif /* _WX_MSW_CHKCONF_H_ */
