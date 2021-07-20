///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/debughlp.h
// Purpose:     wraps dbghelp.h standard file
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2005-01-08 (extracted from msw/crashrpt.cpp)
// Copyright:   (c) 2003-2005 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_DEBUGHLPH_H_
#define _WX_MSW_DEBUGHLPH_H_

#include "wx/dynlib.h"

#include "wx/msw/wrapwin.h"
#ifndef __WXWINCE__
#ifdef __VISUALC__
    // Disable a warning that we can do nothing about: we get it at least for
    // imagehlp.h from 8.1 Windows kit when using VC14.
    #pragma warning(push)

    // 'typedef ': ignored on left of '' when no variable is declared
    #pragma warning(disable:4091)
#endif

#include <imagehlp.h>

#ifdef __VISUALC__
  #pragma warning(pop)
#endif
#endif // __WXWINCE__
#include "wx/msw/private.h"

// All known versions of imagehlp.h define API_VERSION_NUMBER but it's not
// documented, so deal with the possibility that it's not defined just in case.
#ifndef API_VERSION_NUMBER
    #define API_VERSION_NUMBER 0
#endif

#endif // _WX_MSW_DEBUGHLPH_H_

