///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/evtloopconsole.cpp
// Purpose:     wxConsoleEventLoop class for Windows
// Author:      Vadim Zeitlin
// Modified by:
// Created:     01.06.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/msw/wrapwin.h"
#endif //WX_PRECOMP

#include "wx/evtloop.h"

// ============================================================================
// wxMSWEventLoopBase implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ctor/dtor
// ----------------------------------------------------------------------------

wxMSWEventLoopBase::wxMSWEventLoopBase()
{
    m_shouldExit = false;
    m_exitcode = 0;
}

// ----------------------------------------------------------------------------
// wxEventLoop message processing dispatching
// ----------------------------------------------------------------------------

bool wxMSWEventLoopBase::Pending() const
{
    MSG msg;
    return ::PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE) != 0;
}

bool wxMSWEventLoopBase::GetNextMessage(WXMSG* msg)
{
    const BOOL rc = ::GetMessage(msg, NULL, 0, 0);

    if ( rc == 0 )
    {
        // got WM_QUIT
        return false;
    }

    // still break from the loop
    if ( rc == -1 )
        return false;

    return true;
}

// ============================================================================
// wxConsoleEventLoop implementation
// ============================================================================

#if wxUSE_CONSOLE_EVENTLOOP

void wxConsoleEventLoop::WakeUp()
{
#if wxUSE_THREADS
    wxWakeUpMainThread();
#endif
}

bool wxConsoleEventLoop::Dispatch()
{
    MSG msg;
    if ( !GetNextMessage(&msg) )
        return false;

    ::DispatchMessage(&msg);

    return !m_shouldExit;
}

#endif // wxUSE_CONSOLE_EVENTLOOP
