///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/apptbase.h
// Purpose:     declaration of wxAppTraits for MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.06.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_APPTBASE_H_
#define _WX_MSW_APPTBASE_H_

// ----------------------------------------------------------------------------
// wxAppTraits: the MSW version adds extra hooks needed by MSW-only code
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxAppTraits : public wxAppTraitsBase
{
public:
#if wxUSE_THREADS
    // wxThread helpers
    // ----------------
    // wait for the handle to be signaled, return WAIT_OBJECT_0 if it is or, in
    // the GUI code, WAIT_OBJECT_0 + 1 if a Windows message arrived
    virtual WXDWORD WaitForThread(WXHANDLE hThread, int flags) = 0;
#endif // wxUSE_THREADS
protected:
#if wxUSE_THREADS
    // implementation of WaitForThread() for the console applications which is
    // also used by the GUI code if it doesn't [yet|already] dispatch events
    WXDWORD DoSimpleWaitForThread(WXHANDLE hThread);
#endif // wxUSE_THREADS
};

#endif // _WX_MSW_APPTBASE_H_

