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
#include "wx/platinfo.h"

class WXDLLIMPEXP_FWD_BASE wxArrayString;
class WXDLLIMPEXP_FWD_BASE wxConfigBase;
class WXDLLIMPEXP_FWD_BASE wxEventLoopBase;
class WXDLLIMPEXP_FWD_BASE wxObject;
class WXDLLIMPEXP_FWD_CORE wxRendererNative;
class WXDLLIMPEXP_FWD_BASE wxStandardPaths;
class WXDLLIMPEXP_FWD_BASE wxString;
class WXDLLIMPEXP_FWD_BASE wxTimer;
class WXDLLIMPEXP_FWD_BASE wxTimerImpl;

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

    // functions returning port-specific information
    // ------------------------------------------------------------------------

    // return information about the (native) toolkit currently used and its
    // runtime (not compile-time) version.
    // returns wxPORT_BASE for console applications and one of the remaining
    // wxPORT_* values for GUI applications.
    virtual wxPortId GetToolkitVersion(int *majVer = NULL, int *minVer = NULL) const = 0;

    // return the name of the Desktop Environment such as
    // "KDE" or "GNOME". May return an empty string.
    virtual wxString GetDesktopEnvironment() const = 0;

    // returns a short string to identify the block of the standard command
    // line options parsed automatically by current port: if this string is
    // empty, there are no such options, otherwise the function also fills
    // passed arrays with the names and the descriptions of those options.
    virtual wxString GetStandardCmdLineOptions(wxArrayString& names,
                                               wxArrayString& desc) const
    {
        wxUnusedVar(names);
        wxUnusedVar(desc);

        return wxEmptyString;
    }
};

// ----------------------------------------------------------------------------
// include the platform-specific version of the class
// ----------------------------------------------------------------------------

// NB:  test for __UNIX__ before __WXMAC__ as under Darwin we want to use the
//      Unix code (and otherwise __UNIX__ wouldn't be defined)
// ABX: check __WIN32__ instead of __WXMSW__ for the same MSWBase in any Win32 port
#if defined(__WIN32__)
    #include "wx/msw/apptbase.h"
#elif defined(__UNIX__) && !defined(__EMX__)
    #include "wx/unix/apptbase.h"
#elif defined(__OS2__)
    #include "wx/os2/apptbase.h"
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

    // the GetToolkitVersion for console application is always the same
    virtual wxPortId GetToolkitVersion(int *verMaj = NULL, int *verMin = NULL) const
    {
        // no toolkits (wxBase is for console applications without GUI support)
        // NB: zero means "no toolkit", -1 means "not initialized yet"
        //     so we must use zero here!
        if (verMaj) *verMaj = 0;
        if (verMin) *verMin = 0;
        return wxPORT_BASE;
    }

    virtual wxString GetDesktopEnvironment() const { return wxEmptyString; }
};

// ----------------------------------------------------------------------------
// include the platform-specific version of the classes above
// ----------------------------------------------------------------------------

// ABX: check __WIN32__ instead of __WXMSW__ for the same MSWBase in any Win32 port
#if defined(__WIN32__)
    #include "wx/msw/apptrait.h"
#elif defined(__OS2__)
    #include "wx/os2/apptrait.h"
#elif defined(__UNIX__)
    #include "wx/unix/apptrait.h"
#elif defined(__DOS__)
    #include "wx/msdos/apptrait.h"
#else
    class wxConsoleAppTraits: public wxConsoleAppTraitsBase
    {
    };
#endif // platform

#endif // _WX_APPTRAIT_H_

