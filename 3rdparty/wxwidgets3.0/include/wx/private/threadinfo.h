///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/threadinfo.h
// Purpose:     declaration of wxThreadSpecificInfo: thread-specific information
// Author:      Vadim Zeitlin
// Created:     2009-07-13
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_THREADINFO_H_
#define _WX_PRIVATE_THREADINFO_H_

#include "wx/defs.h"

// ----------------------------------------------------------------------------
// wxThreadSpecificInfo: contains all thread-specific information used by wx
// ----------------------------------------------------------------------------

// Group all thread-specific information we use (e.g. the active wxLog target)
// a in this class to avoid consuming more TLS slots than necessary as there is
// only a limited number of them.
class wxThreadSpecificInfo
{
public:
#if wxUSE_THREADS
    // Cleans up storage for the current thread. Should be called when a thread
    // is being destroyed. If it's not called, the only bad thing that happens
    // is that the memory is deallocated later, on process termination.
    static void ThreadCleanUp();
#endif

private:
    wxThreadSpecificInfo() {}
};

#endif // _WX_PRIVATE_THREADINFO_H_

