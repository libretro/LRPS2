///////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/apptbase.h
// Purpose:     declaration of wxAppTraits for Unix systems
// Author:      Vadim Zeitlin
// Modified by:
// Created:     23.06.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_APPTBASE_H_
#define _WX_UNIX_APPTBASE_H_

#include "wx/evtloop.h"
#include "wx/evtloopsrc.h"

class wxFDIOManager;
class wxEventLoopSourcesManagerBase;

// ----------------------------------------------------------------------------
// wxAppTraits: the Unix version adds extra hooks needed by Unix code
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxAppTraits : public wxAppTraitsBase
{
public:
    // wxExecute() support methods
    // ---------------------------

#if wxUSE_CONSOLE_EVENTLOOP
    // Return a non-NULL pointer to the object responsible for managing the
    // event loop sources in this kind of application.
    virtual wxEventLoopSourcesManagerBase* GetEventLoopSourcesManager();
#endif // wxUSE_CONSOLE_EVENTLOOP
};

#endif // _WX_UNIX_APPTBASE_H_

