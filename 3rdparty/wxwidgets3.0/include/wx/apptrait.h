///////////////////////////////////////////////////////////////////////////////
// Name:        wx/apptrait.h
// Purpose:     declaration of wxAppTraits and derived classes
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.06.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_APPTRAIT_H_
#define _WX_APPTRAIT_H_

#include "wx/string.h"

class WXDLLIMPEXP_FWD_BASE wxArrayString;
class WXDLLIMPEXP_FWD_BASE wxEventLoopBase;
class WXDLLIMPEXP_FWD_BASE wxObject;
class WXDLLIMPEXP_FWD_BASE wxString;

// ----------------------------------------------------------------------------
// wxAppTraits: this class defines various configurable aspects of wxApp
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxAppTraitsBase
{
public:
    // needed since this class declares virtual members
    virtual ~wxAppTraitsBase() { }

    // functions abstracting differences between GUI and console modes
    // ------------------------------------------------------------------------

    // create a new, port specific, instance of the event loop used by wxApp
    virtual wxEventLoopBase *CreateEventLoop() = 0;

#if wxUSE_THREADS
    virtual void MutexGuiEnter();
    virtual void MutexGuiLeave();
#endif
};

// ----------------------------------------------------------------------------
// include the platform-specific version of the class
// ----------------------------------------------------------------------------

#if defined(__UNIX__)
    #include "wx/unix/apptbase.h"
#else // no platform-specific methods to add to wxAppTraits
    // wxAppTraits must be a class because it was forward declared as class
    class WXDLLIMPEXP_BASE wxAppTraits : public wxAppTraitsBase
    {
    };
#endif // platform

// ============================================================================
// standard traits for console and GUI applications
// ============================================================================

// ----------------------------------------------------------------------------
// wxConsoleAppTraitsBase: wxAppTraits implementation for the console apps
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxConsoleAppTraitsBase : public wxAppTraits
{
public:
#if !wxUSE_CONSOLE_EVENTLOOP
    virtual wxEventLoopBase *CreateEventLoop() { return NULL; }
#endif // !wxUSE_CONSOLE_EVENTLOOP
};

// ----------------------------------------------------------------------------
// include the platform-specific version of the classes above
// ----------------------------------------------------------------------------
    class wxConsoleAppTraits: public wxConsoleAppTraitsBase
    {
	    virtual wxEventLoopBase *CreateEventLoop();
    };

#endif // _WX_APPTRAIT_H_

