///////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/apptrait.h
// Purpose:     standard implementations of wxAppTraits for Unix
// Author:      Vadim Zeitlin
// Modified by:
// Created:     23.06.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_APPTRAIT_H_
#define _WX_UNIX_APPTRAIT_H_

// ----------------------------------------------------------------------------
// wxGUI/ConsoleAppTraits: must derive from wxAppTraits, not wxAppTraitsBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxConsoleAppTraits : public wxConsoleAppTraitsBase
{
public:
#if wxUSE_CONSOLE_EVENTLOOP
    virtual wxEventLoopBase *CreateEventLoop();
#endif // wxUSE_CONSOLE_EVENTLOOP
};

#endif // _WX_UNIX_APPTRAIT_H_

