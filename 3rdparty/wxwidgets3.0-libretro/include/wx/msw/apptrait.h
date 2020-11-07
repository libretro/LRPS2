///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/apptrait.h
// Purpose:     class implementing wxAppTraits for MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     21.06.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_APPTRAIT_H_
#define _WX_MSW_APPTRAIT_H_

// ----------------------------------------------------------------------------
// wxGUI/ConsoleAppTraits: must derive from wxAppTraits, not wxAppTraitsBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxConsoleAppTraits : public wxConsoleAppTraitsBase
{
public:
    virtual wxEventLoopBase *CreateEventLoop();
    virtual void *BeforeChildWaitLoop();
    virtual void AfterChildWaitLoop(void *data);
#if wxUSE_TIMER
    virtual wxTimerImpl *CreateTimerImpl(wxTimer *timer);
#endif // wxUSE_TIMER
#if wxUSE_THREADS
    virtual bool DoMessageFromThreadWait();
    virtual WXDWORD WaitForThread(WXHANDLE hThread, int flags);
#endif // wxUSE_THREADS
#ifndef __WXWINCE__
    virtual bool CanUseStderr() { return true; }
    virtual bool WriteToStderr(const wxString& text);
#endif // !__WXWINCE__
};

#endif // _WX_MSW_APPTRAIT_H_
