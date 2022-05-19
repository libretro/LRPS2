///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/evtloopconsole.h
// Purpose:     wxConsoleEventLoop class for Windows
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-07-31
// Copyright:   (c) 2003-2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_EVTLOOPCONSOLE_H_
#define _WX_MSW_EVTLOOPCONSOLE_H_

class WXDLLIMPEXP_BASE wxMSWEventLoopBase : public wxEventLoopManual
{
public:
    wxMSWEventLoopBase();

    // implement base class pure virtuals
    virtual bool Pending() const;
};

#if wxUSE_CONSOLE_EVENTLOOP

class WXDLLIMPEXP_BASE wxConsoleEventLoop : public wxMSWEventLoopBase
{
public:
    wxConsoleEventLoop() { }

    // override/implement base class virtuals
    virtual bool Dispatch();
    virtual void WakeUp();
    virtual bool YieldFor(long WXUNUSED(eventsToProcess)) { return true; }
};

#endif // wxUSE_CONSOLE_EVENTLOOP

#endif // _WX_MSW_EVTLOOPCONSOLE_H_
