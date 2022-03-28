/* lib/wx/include/gtk3-unicode-3.0/wx/setup.h.  Generated from setup.h.in by configure.  */
/* This define (__WX_SETUP_H__) is used both to ensure setup.h is included
 * only once and to indicate that we are building using configure. */
#ifndef __WX_SETUP_H__
#define __WX_SETUP_H__
#if __UNIX__
/* Define if ssize_t type is available.  */
#define HAVE_SSIZE_T 1

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define this to get extra features from GNU libc. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

#ifndef wxUSE_UNICODE
    #define wxUSE_UNICODE 1
#endif

#define wxUSE_WCHAR_T 1

#define wxUSE_THREADS 1

#define wxUSE_STREAMS 1

#define wxUSE_PRINTF_POS_PARAMS 1

#define wxUSE_COMPILER_TLS 1


#define wxUSE_STL 0

#if (defined(_MSC_VER) && _MSC_VER < 1200)
    #define wxUSE_STD_DEFAULT  0
#else
    #define wxUSE_STD_DEFAULT  1
#endif

#define wxUSE_STD_IOSTREAM wxUSE_STD_DEFAULT

#define wxUSE_STD_STRING wxUSE_STD_DEFAULT

#define wxUSE_STD_STRING_CONV_IN_WXSTRING wxUSE_STL

#define wxUSE_IOSTREAMH     0



#define wxUSE_LONGLONG 1

#define wxUSE_CONSOLE_EVENTLOOP 1

#define wxUSE_FILE 1
#define wxUSE_FFILE 1

#define wxUSE_TEXTBUFFER 1

#define wxUSE_TEXTFILE 1

#define wxUSE_DATETIME 1

#define wxUSE_IPV6          0

#define wxUSE_APPLE_IEEE 1

#define wxUSE_PROTOCOL 1

#define wxUSE_PROTOCOL_FILE 1
#define wxUSE_PROTOCOL_FTP 1
#define wxUSE_PROTOCOL_HTTP 1

#ifdef __WXMSW__
#define wxUSE_AUTOID_MANAGEMENT 0
#else
#define wxUSE_AUTOID_MANAGEMENT 0
#endif

#define wxUSE_WIN_METAFILES_ALWAYS  0

/* --- end common options --- */
#if __unix__
/*
 * Unix-specific options
 */
#define wxUSE_SELECT_DISPATCHER 1
#define wxUSE_EPOLL_DISPATCHER 1

#define wxUSE_UNICODE_UTF8 0
#define wxUSE_UTF8_LOCALE_ONLY 0
#endif
#ifndef _WIN32
/*
 * Define if your compiler supports the explicit keyword
 */
#define HAVE_EXPLICIT 1

/*
 * Define if your compiler has C99 va_copy
 */
#define HAVE_VA_COPY 1

/*
 * Define if va_list type is an array
 */
/* #undef VA_LIST_IS_ARRAY */

/*
 * Define if your compiler has std::wstring
 */
#define HAVE_STD_WSTRING 1
/*
 * Define if your compiler has compliant std::string::compare
 */
/* #undef HAVE_STD_STRING_COMPARE */
/*
 * Define if your compiler has <hash_map>
 */
/* #undef HAVE_HASH_MAP */
/*
 * Define if your compiler has <ext/hash_map>
 */
/* #undef HAVE_EXT_HASH_MAP */
/*
 * Define if your compiler has std::hash_map/hash_set
 */
/* #undef HAVE_STD_HASH_MAP */
/*
 * Define if your compiler has __gnu_cxx::hash_map/hash_set
 */
/* #undef HAVE_GNU_CXX_HASH_MAP */

/*
 * Define if your compiler has std::unordered_map
 */
/* #undef HAVE_STD_UNORDERED_MAP */

/*
 * Define if your compiler has std::unordered_set
 */
/* #undef HAVE_STD_UNORDERED_SET */

/*
 * Define if your compiler has std::tr1::unordered_map
 */
/* #undef HAVE_TR1_UNORDERED_MAP */

/*
 * Define if your compiler has std::tr1::unordered_set
 */
/* #undef HAVE_TR1_UNORDERED_SET */

/*
 * Define if your compiler has <tr1/type_traits>
 */
/* #undef HAVE_TR1_TYPE_TRAITS */

/*
 * Define if your compiler has <type_traits>
 */
#define HAVE_TYPE_TRAITS 1

/*
 * Define if the compiler supports simple visibility declarations.
 */
#define HAVE_VISIBILITY 1

/*
 * Define if the compiler supports GCC's atomic memory access builtins
 */
#define HAVE_GCC_ATOMIC_BUILTINS 1

/*
 * Define if compiler's visibility support in libstdc++ is broken
 */
/* #undef HAVE_BROKEN_LIBSTDCXX_VISIBILITY */

/*
 * The built-in regex supports advanced REs in additional to POSIX's basic
 * and extended. Your system regex probably won't support this, and in this
 * case WX_NO_REGEX_ADVANCED should be defined.
 */
#define WX_NO_REGEX_ADVANCED 1

/*
 * Define if you have pthread_cleanup_push/pop()
 */
#define wxHAVE_PTHREAD_CLEANUP 1
/*
 * Define if compiler has __thread keyword.
 */
/* #undef HAVE___THREAD_KEYWORD */
/*
 * Define if large (64 bit file offsets) files are supported.
 */
#define HAVE_LARGEFILE_SUPPORT 1

/*
 * wxMediaCtrl on OS X
 */
/* #undef wxOSX_USE_QTKIT */

/*
 * Objective-C class name uniquifying
 */
#define wxUSE_OBJC_UNIQUIFYING 0

/*
 * The const keyword is being introduced more in wxWindows.
 * You can use this setting to maintain backward compatibility.
 * If 0: will use const wherever possible.
 * If 1: will use const only where necessary
 *       for precompiled headers to work.
 * If 2: will be totally backward compatible, but precompiled
 *       headers may not work and program size will be larger.
 */
#define CONST_COMPATIBILITY 0

/* define with the name of timezone variable */
#define WX_TIMEZONE timezone

/* The type of 3rd argument to getsockname() - usually size_t or int */
#define WX_SOCKLEN_T socklen_t

/* The type of 5th argument to getsockopt() - usually size_t or int */
#define SOCKOPTLEN_T socklen_t

/* The type of statvfs(2) argument */
#define WX_STATFS_T struct statfs

/* The signal handler prototype */
#define wxTYPE_SA_HANDLER int

/* gettimeofday() usually takes 2 arguments, but some really old systems might
 * have only one, in which case define WX_GETTIMEOFDAY_NO_TZ */
/* #undef WX_GETTIMEOFDAY_NO_TZ */

/* struct tm doesn't always have the tm_gmtoff field, define this if it does */
#define WX_GMTOFF_IN_TM 1

/* Define if you have poll(2) function */
/* #undef HAVE_POLL */

/* Define if you have pw_gecos field in struct passwd */
#define HAVE_PW_GECOS 1

/* Define if you have __cxa_demangle() in <cxxabi.h> */
#define HAVE_CXA_DEMANGLE 1

/* Define if you have dlopen() */
#define HAVE_DLOPEN 1

/* Define if you have gettimeofday() */
#define HAVE_GETTIMEOFDAY 1

/* Define if fsync() is available */
#define HAVE_FSYNC 1

/* Define if round() is available */
#define HAVE_ROUND 1

/* Define if you have ftime() */
/* #undef HAVE_FTIME */

/* Define if you have nanosleep() */
#define HAVE_NANOSLEEP 1

/* Define if you have sched_yield */
#define HAVE_SCHED_YIELD 1

/* Define if you have pthread_mutexattr_t and functions to work with it */
#define HAVE_PTHREAD_MUTEXATTR_T 1

/* Define if you have pthread_mutexattr_settype() declaration */
#define HAVE_PTHREAD_MUTEXATTR_SETTYPE_DECL 1

/* Define if you have PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP */
/* #undef HAVE_PTHREAD_RECURSIVE_MUTEX_INITIALIZER */

/* Define if you have pthread_cancel */
#define HAVE_PTHREAD_CANCEL 1

/* Define if you have pthread_mutex_timedlock */
#define HAVE_PTHREAD_MUTEX_TIMEDLOCK 1

/* Define if you have pthread_attr_setstacksize */
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE 1

/* Define if you have shl_load() */
/* #undef HAVE_SHL_LOAD */

/* Define if you have snprintf() */
#define HAVE_SNPRINTF 1

/* Define if you have snprintf() declaration in the header */
#define HAVE_SNPRINTF_DECL 1

/* Define if you have a snprintf() which supports positional arguments
   (defined in the unix98 standard) */
#define HAVE_UNIX98_PRINTF 1

/* define if you have statfs function */
#define HAVE_STATFS 1

/* define if you have statfs prototype */
#define HAVE_STATFS_DECL 1

/* define if you have statvfs function */
/* #undef HAVE_STATVFS */

/* Define if you have strtoull() and strtoll() */
#define HAVE_STRTOULL 1

/* Define if you have all functions to set thread priority */
#define HAVE_THREAD_PRIORITY_FUNCTIONS 1

/* Define if you have vsnprintf() */
#define HAVE_VSNPRINTF 1

/* Define if you have vsnprintf() declaration in the header */
#define HAVE_VSNPRINTF_DECL 1

/* Define if you have a _broken_ vsnprintf() declaration in the header,
 * with 'char*' for the 3rd parameter instead of 'const char*' */
/* #undef HAVE_BROKEN_VSNPRINTF_DECL */

/* Define if you have a _broken_ vsscanf() declaration in the header,
 * with 'char*' for the 1st parameter instead of 'const char*' */
/* #undef HAVE_BROKEN_VSSCANF_DECL */

/* Define if you have vsscanf() */
#define HAVE_VSSCANF 1

/* Define if you have vsscanf() declaration in the header */
#define HAVE_VSSCANF_DECL 1

/* Define if you have usleep() */
/* #undef HAVE_USLEEP */

/* Define if you have wcscasecmp() function  */
#define HAVE_WCSCASECMP 1

/* Define if you have wcsncasecmp() function  */
#define HAVE_WCSNCASECMP 1

/* Define if you have wcslen function  */
#define HAVE_WCSLEN 1

/* Define if you have wcsdup function  */
#define HAVE_WCSDUP 1

/* Define if you have wcsftime() function  */
#define HAVE_WCSFTIME 1

/* Define if you have strnlen() function */
#define HAVE_STRNLEN 1

/* Define if you have wcsnlen() function */
#define HAVE_WCSNLEN 1

/* Define if you have wcstoull() and wcstoll() */
/* #undef HAVE_WCSTOULL */

/* The number of bytes in a wchar_t.  */
#define SIZEOF_WCHAR_T 4

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a pointer.  */
#define SIZEOF_VOID_P 8

/* The number of bytes in a long.  */
#define SIZEOF_LONG 8

/* The number of bytes in a long long.  */
#define SIZEOF_LONG_LONG 8

/* The number of bytes in a short.  */
#define SIZEOF_SHORT 2

/* The number of bytes in a size_t.  */
#define SIZEOF_SIZE_T 8

/* Define if size_t on your machine is the same type as unsigned int. */
/* #undef wxSIZE_T_IS_UINT */

/* Define if size_t on your machine is the same type as unsigned long. */
#define wxSIZE_T_IS_ULONG 1

/* Define if wchar_t is distinct type in your compiler. */
#define wxWCHAR_T_IS_REAL_TYPE 1

/* Define if you have the dlerror function.  */
#define HAVE_DLERROR 1

/* Define if you have Posix fnctl() function. */
#define HAVE_FCNTL 1

/* Define if you have BSD flock() function. */
/* #undef HAVE_FLOCK */

/* Define if you have getaddrinfo function. */
/* #undef HAVE_GETADDRINFO */

/* Define if you have a gethostbyname_r function taking 6 arguments. */
#define HAVE_FUNC_GETHOSTBYNAME_R_6 1

/* Define if you have a gethostbyname_r function taking 5 arguments. */
/* #undef HAVE_FUNC_GETHOSTBYNAME_R_5 */

/* Define if you have a gethostbyname_r function taking 3 arguments. */
/* #undef HAVE_FUNC_GETHOSTBYNAME_R_3 */

/* Define if you only have a gethostbyname function */
/* #undef HAVE_GETHOSTBYNAME */

/* Define if you have the gethostname function.  */
/* #undef HAVE_GETHOSTNAME */

/* Define if you have a getservbyname_r function taking 6 arguments. */
#define HAVE_FUNC_GETSERVBYNAME_R_6 1

/* Define if you have a getservbyname_r function taking 5 arguments. */
/* #undef HAVE_FUNC_GETSERVBYNAME_R_5 */

/* Define if you have a getservbyname_r function taking 4 arguments. */
/* #undef HAVE_FUNC_GETSERVBYNAME_R_4 */

/* Define if you only have a getservbyname function */
/* #undef HAVE_GETSERVBYNAME */

/* Define if you have the gmtime_r function.  */
#define HAVE_GMTIME_R 1

/* Define if you have the inet_addr function.  */
#define HAVE_INET_ADDR 1

/* Define if you have the inet_aton function.  */
#define HAVE_INET_ATON 1

/* Define if you have the localtime_r function.  */
#define HAVE_LOCALTIME_R 1

/* Define if you have the mktemp function.  */
/* #undef HAVE_MKTEMP */

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the putenv function.  */
/* #undef HAVE_PUTENV */

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have strtok_r function. */
/* #define HAVE_STRTOK_R */

/* Define if you have thr_setconcurrency function */
/* #undef HAVE_THR_SETCONCURRENCY */

/* Define if you have pthread_setconcurrency function */
#define HAVE_PTHREAD_SET_CONCURRENCY 1

/* Define if you have the uname function.  */
#define HAVE_UNAME 1

/* Define if you have the unsetenv function.  */
#define HAVE_UNSETENV 1

/* Define if you have the <X11/XKBlib.h> header file.  */
#define HAVE_X11_XKBLIB_H 1

/* Define if you have the <X11/extensions/xf86vmode.h> header file.  */
#define HAVE_X11_EXTENSIONS_XF86VMODE_H 1

/* Define if you have the <sched.h> header file.  */
#define HAVE_SCHED_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <fcntl.h> header file.  */
/* #undef HAVE_FCNTL_H */

/* Define if you have the <wchar.h> header file.  */
#define HAVE_WCHAR_H 1

/* Define if you have the <wcstr.h> header file.  */
/* #undef HAVE_WCSTR_H */

/* Define if you have <widec.h> (Solaris only) */
/* #undef HAVE_WIDEC_H */

/* Define if you have the <iconv.h> header file and iconv() symbol.  */
#define HAVE_ICONV 1

/* Define as "const" if the declaration of iconv() needs const.  */
#define ICONV_CONST 

/* Define if you have the <langinfo.h> header file.  */
#define HAVE_LANGINFO_H 1

/* Define if you have the <w32api.h> header file (mingw,cygwin).  */
/* #undef HAVE_W32API_H */

/* Define if you have the <sys/soundcard.h> header file. */
#define HAVE_SYS_SOUNDCARD_H 1

/* Define if you have wcsrtombs() function */
#define HAVE_WCSRTOMBS 1

/* Define this if you have putws() */
/* #undef HAVE_PUTWS */

/* Define this if you have fputws() */
#define HAVE_FPUTWS 1

/* Define this if you have wprintf() and related functions */
#define HAVE_WPRINTF 1

/* Define this if you have vswprintf() and related functions */
#define HAVE_VSWPRINTF 1

/* Define this if you have _vsnwprintf */
/* #undef HAVE__VSNWPRINTF */

/* vswscanf() */
#define HAVE_VSWSCANF 1

/* Define if fseeko and ftello are available.  */
#define HAVE_FSEEKO 1

/* Define this if you are using gtk and gdk contains support for X11R6 XIM */
/* #undef HAVE_XIM */

/* Define this if you have X11/extensions/shape.h */
/* #undef HAVE_XSHAPE */

/* Define this if you have type SPBCDATA */
/* #undef HAVE_SPBCDATA */

/* Define if you have pango_font_family_is_monospace() (Pango >= 1.3.3) */
/* #undef HAVE_PANGO_FONT_FAMILY_IS_MONOSPACE */

/* Define if you have Pango xft support */
/* #undef HAVE_PANGO_XFT */

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have abi::__forced_unwind in your <cxxabi.h>. */
#define HAVE_ABI_FORCEDUNWIND 1

/* Define if fdopen is available.  */
#define HAVE_FDOPEN 1

/* Define if sysconf is available. */
#define HAVE_SYSCONF 1

/* Define if getpwuid_r is available. */
#define HAVE_GETPWUID_R 1

/* Define if getgrgid_r is available. */
#define HAVE_GETGRGID_R 1

/* Define if setpriority() is available. */
#define HAVE_SETPRIORITY 1

/* Define if locale_t is available */
/* #undef HAVE_LOCALE_T */

#endif

#endif /* __WX_SETUP_H__ */

