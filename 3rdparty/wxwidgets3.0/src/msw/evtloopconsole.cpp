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
    m_exitcode = 0;
}

// ----------------------------------------------------------------------------
// wxEventLoop message processing dispatching
// ----------------------------------------------------------------------------

bool wxMSWEventLoopBase::Pending() const
{
	return false;
}

// ============================================================================
// wxConsoleEventLoop implementation
// ============================================================================

#if wxUSE_CONSOLE_EVENTLOOP

void wxConsoleEventLoop::WakeUp()
{
}

bool wxConsoleEventLoop::Dispatch()
{
    return true;
}

#endif // wxUSE_CONSOLE_EVENTLOOP
