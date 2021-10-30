///////////////////////////////////////////////////////////////////////////////
// Name:        wx/eventfilter.h
// Purpose:     wxEventFilter class declaration.
// Author:      Vadim Zeitlin
// Created:     2011-11-21
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EVENTFILTER_H_
#define _WX_EVENTFILTER_H_

#include "wx/defs.h"

class WXDLLIMPEXP_FWD_BASE wxEvent;
class WXDLLIMPEXP_FWD_BASE wxEvtHandler;

// ----------------------------------------------------------------------------
// wxEventFilter is used with wxEvtHandler::AddFilter() and ProcessEvent().
// ----------------------------------------------------------------------------

class wxEventFilter
{
public:
    wxEventFilter()
    {
        m_next = NULL;
    }

    virtual ~wxEventFilter()
    {
    }

private:
    // Objects of this class are made to be stored in a linked list in
    // wxEvtHandler so put the next node ponter directly in the class itself.
    wxEventFilter* m_next;

    // And provide access to it for wxEvtHandler [only].
    friend class wxEvtHandler;

    wxDECLARE_NO_COPY_CLASS(wxEventFilter);
};

#endif // _WX_EVENTFILTER_H_
