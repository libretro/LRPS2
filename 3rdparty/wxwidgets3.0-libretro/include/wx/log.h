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

#if wxUSE_GUI
    class WXDLLIMPEXP_FWD_CORE wxFrame;
#endif // wxUSE_GUI

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

#if wxUSE_GUI
    wxDEFINE_EMPTY_LOG_FUNCTION(Status);
    wxDEFINE_EMPTY_LOG_FUNCTION2(Status, wxFrame *);
#endif // wxUSE_GUI

// Empty Class to fake wxLogNull
class WXDLLIMPEXP_BASE wxLogNull
{
public:
    wxLogNull() { }
};

// Dummy macros to replace some functions.
#define wxSysErrorCode() (unsigned long)0
#define wxSysErrorMsg( X ) (const wxChar*)NULL

// Fake symbolic trace masks... for those that are used frequently
#define wxTRACE_OleCalls wxEmptyString // OLE interface calls

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

    #define wxVLogDebug(fmt, valist) wxLogNop()

    #ifdef HAVE_VARIADIC_MACROS
        #define wxLogDebug(fmt, ...) wxLogNop()
    #else // !HAVE_VARIADIC_MACROS
        WX_DEFINE_VARARG_FUNC_NOP(wxLogDebug, 1, (const wxFormatString&))
    #endif

    #define wxVLogTrace(mask, fmt, valist) wxLogNop()

    #ifdef HAVE_VARIADIC_MACROS
        #define wxLogTrace(mask, fmt, ...) wxLogNop()
    #else // !HAVE_VARIADIC_MACROS
        #if WXWIN_COMPATIBILITY_2_8
        WX_DEFINE_VARARG_FUNC_NOP(wxLogTrace, 2, (wxTraceMask, const wxFormatString&))
        #endif
        WX_DEFINE_VARARG_FUNC_NOP(wxLogTrace, 2, (const wxString&, const wxFormatString&))
        #ifdef __WATCOMC__
        // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
        WX_DEFINE_VARARG_FUNC_NOP(wxLogTrace, 2, (const char*, const char*))
        WX_DEFINE_VARARG_FUNC_NOP(wxLogTrace, 2, (const wchar_t*, const wchar_t*))
        #endif
    #endif // HAVE_VARIADIC_MACROS/!HAVE_VARIADIC_MACROS

// wxLogFatalError helper: show the (fatal) error to the user in a safe way,
// i.e. without using wxMessageBox() for example because it could crash
void WXDLLIMPEXP_BASE
wxSafeShowMessage(const wxString& title, const wxString& text);

// ----------------------------------------------------------------------------
// debug only logging functions: use them with API name and error code
// ----------------------------------------------------------------------------

    #define wxLogApiError(api, err) wxLogNop()
    #define wxLogLastError(api) wxLogNop()

// wxCocoa has additiional trace masks
#if defined(__WXCOCOA__)
#include "wx/cocoa/log.h"
#endif

#ifdef WX_WATCOM_ONLY_CODE
    #undef WX_WATCOM_ONLY_CODE
#endif

// macro which disables debug logging in release builds: this is done by
// default by wxIMPLEMENT_APP() so usually it doesn't need to be used explicitly
#define wxDISABLE_DEBUG_LOGGING_IN_RELEASE_BUILD()

#endif  // _WX_LOG_H_

