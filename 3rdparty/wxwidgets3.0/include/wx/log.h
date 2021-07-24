/////////////////////////////////////////////////////////////////////////////
// Name:        wx/log.h
// Purpose:     Assorted wxLogXXX functions, and wxLog (sink for logs)
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LOG_H_
#define _WX_LOG_H_

#include "wx/defs.h"
#include "wx/cpp.h"

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

// NB: this is needed even if wxUSE_LOG == 0
typedef unsigned long wxLogLevel;

// the trace masks have been superseded by symbolic trace constants, they're
// for compatibility only and will be removed soon - do NOT use them
#if WXWIN_COMPATIBILITY_2_8
    #define wxTraceMemAlloc 0x0001  // trace memory allocation (new/delete)
    #define wxTraceMessages 0x0002  // trace window messages/X callbacks
    #define wxTraceResAlloc 0x0004  // trace GDI resource allocation
    #define wxTraceRefCount 0x0008  // trace various ref counting operations

    #ifdef  __WINDOWS__
        #define wxTraceOleCalls 0x0100  // OLE interface calls
    #endif

    typedef unsigned long wxTraceMask;
#endif // WXWIN_COMPATIBILITY_2_8

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/string.h"
#include "wx/strvararg.h"

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_BASE wxObject;

#if defined(__WATCOMC__) || defined(__MINGW32__)
    // Mingw has similar problem with wxLogSysError:
    #define WX_WATCOM_OR_MINGW_ONLY_CODE( x )  x
#else
    #define WX_WATCOM_OR_MINGW_ONLY_CODE( x )
#endif

// define macros for defining log functions which do nothing at all
//
// WX_WATCOM_ONLY_CODE is needed to work around
// http://bugzilla.openwatcom.org/show_bug.cgi?id=351
#define wxDEFINE_EMPTY_LOG_FUNCTION(level)                                  \
    WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 1, (const wxFormatString&))     \
    WX_WATCOM_ONLY_CODE(                                                    \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 1, (const char*))           \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 1, (const wchar_t*))        \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 1, (const wxCStrData&))     \
    )                                                                       \
    inline void wxVLog##level(const wxFormatString& WXUNUSED(format),       \
                              va_list WXUNUSED(argptr)) { }                 \

#define wxDEFINE_EMPTY_LOG_FUNCTION2(level, argclass)                       \
    WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 2, (argclass, const wxFormatString&)) \
    WX_WATCOM_ONLY_CODE(                                                    \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 2, (argclass, const char*)) \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 2, (argclass, const wchar_t*)) \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 2, (argclass, const wxCStrData&)) \
    )                                                                       \
    inline void wxVLog##level(argclass WXUNUSED(arg),                       \
                              const wxFormatString& WXUNUSED(format),       \
                              va_list WXUNUSED(argptr)) {}

wxDEFINE_EMPTY_LOG_FUNCTION(FatalError);
wxDEFINE_EMPTY_LOG_FUNCTION(Error);
wxDEFINE_EMPTY_LOG_FUNCTION(SysError);
wxDEFINE_EMPTY_LOG_FUNCTION2(SysError, long);
wxDEFINE_EMPTY_LOG_FUNCTION(Warning);
wxDEFINE_EMPTY_LOG_FUNCTION(Message);
wxDEFINE_EMPTY_LOG_FUNCTION(Info);
wxDEFINE_EMPTY_LOG_FUNCTION(Verbose);

wxDEFINE_EMPTY_LOG_FUNCTION2(Generic, wxLogLevel);

// Empty Class to fake wxLogNull
class WXDLLIMPEXP_BASE wxLogNull
{
public:
    wxLogNull() { }
};

// debug functions can be completely disabled in optimized builds

// if these log functions are disabled, we prefer to define them as (empty)
// variadic macros as this completely removes them and their argument
// evaluation from the object code but if this is not supported by compiler we
// use empty inline functions instead (defining them as nothing would result in
// compiler warnings)
//
// note that making wxVLogDebug/Trace() themselves (empty inline) functions is
// a bad idea as some compilers are stupid enough to not inline even empty
// functions if their parameters are complicated enough, but by defining them
// as an empty inline function we ensure that even dumbest compilers optimise
// them away
#ifdef __BORLANDC__
    // but Borland gives "W8019: Code has no effect" for wxLogNop() so we need
    // to define it differently for it to avoid these warnings (same problem as
    // with wxUnusedVar())
    #define wxLogNop() { }
#else
    inline void wxLogNop() { }
#endif

// ----------------------------------------------------------------------------
// debug only logging functions: use them with API name and error code
// ----------------------------------------------------------------------------
#ifdef WX_WATCOM_ONLY_CODE
    #undef WX_WATCOM_ONLY_CODE
#endif

#endif  // _WX_LOG_H_

