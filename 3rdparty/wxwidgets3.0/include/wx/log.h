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

// ----------------------------------------------------------------------------
// debug only logging functions: use them with API name and error code
// ----------------------------------------------------------------------------
#ifdef WX_WATCOM_ONLY_CODE
    #undef WX_WATCOM_ONLY_CODE
#endif

#endif  // _WX_LOG_H_

