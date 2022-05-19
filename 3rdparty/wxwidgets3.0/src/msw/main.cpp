/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/main.cpp
// Purpose:     WinMain/DllMain
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/event.h"
    #include "wx/app.h"
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/scopeguard.h"

#include "wx/msw/private.h"

// defined in common/init.cpp
extern int wxEntryReal(int& argc, wxChar **argv);
extern int wxEntryCleanupReal(int& argc, wxChar **argv);

// ============================================================================
// implementation: various entry points
// ============================================================================

#if wxUSE_BASE

// ----------------------------------------------------------------------------
// wrapper wxEntry catching all Win32 exceptions occurring in a wx program
// ----------------------------------------------------------------------------

// wrap real wxEntry in a try-except block to be able to call
// OnFatalException() if necessary

int wxEntry(int& argc, wxChar **argv)
{
    return wxEntryReal(argc, argv);
}

#endif // wxUSE_BASE

// ----------------------------------------------------------------------------
// global HINSTANCE
// ----------------------------------------------------------------------------

#if wxUSE_BASE

HINSTANCE wxhInstance = 0;

extern "C" HINSTANCE wxGetInstance()
{
    return wxhInstance;
}

void wxSetInstance(HINSTANCE hInst)
{
    wxhInstance = hInst;
}

#endif // wxUSE_BASE
