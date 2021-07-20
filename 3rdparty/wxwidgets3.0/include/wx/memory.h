/////////////////////////////////////////////////////////////////////////////
// Name:        wx/memory.h
// Purpose:     Memory operations
// Author:      Arthur Seaton, Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MEMORY_H_
#define _WX_MEMORY_H_

#include "wx/defs.h"
#include "wx/string.h"
#include "wx/msgout.h"

#define WXDEBUG_DUMPDELAYCOUNTER

// Borland C++ Builder 6 seems to have troubles with inline functions (see bug
// 819700)
#if 0
    inline void wxTrace(const wxChar *WXUNUSED(fmt)) {}
    inline void wxTraceLevel(int WXUNUSED(level), const wxChar *WXUNUSED(fmt)) {}
#else
    #define wxTrace(fmt)
    #define wxTraceLevel(l, fmt)
#endif

#define WXTRACE true ? (void)0 : wxTrace
#define WXTRACELEVEL true ? (void)0 : wxTraceLevel

#endif // _WX_MEMORY_H_
