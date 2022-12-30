///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/fdiodispatcher.cpp
// Purpose:     Implementation of common wxFDIODispatcher methods
// Author:      Vadim Zeitlin
// Created:     2007-05-13
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/module.h"
#endif //WX_PRECOMP

#include "wx/private/fdiodispatcher.h"

#include "wx/private/selectdispatcher.h"
#ifdef __UNIX__
    #include "wx/unix/private/epolldispatcher.h"
#endif

wxFDIODispatcher *gs_dispatcher = NULL;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFDIODispatcher
// ----------------------------------------------------------------------------

/* static */
wxFDIODispatcher *wxFDIODispatcher::Get()
{
    if ( !gs_dispatcher )
    {
#if wxUSE_EPOLL_DISPATCHER
        gs_dispatcher = wxEpollDispatcher::Create();
        if ( !gs_dispatcher )
#endif // wxUSE_EPOLL_DISPATCHER
#if wxUSE_SELECT_DISPATCHER
            gs_dispatcher = new wxSelectDispatcher();
#endif // wxUSE_SELECT_DISPATCHER
    }
    return gs_dispatcher;
}

/* static */
void wxFDIODispatcher::DispatchPending()
{
    if ( gs_dispatcher )
        gs_dispatcher->Dispatch(0);
}

// ----------------------------------------------------------------------------
// wxMappedFDIODispatcher
// ----------------------------------------------------------------------------

wxFDIOHandler *wxMappedFDIODispatcher::FindHandler(int fd) const
{
    const wxFDIOHandlerMap::const_iterator it = m_handlers.find(fd);

    return it == m_handlers.end() ? NULL : it->second.handler;
}


bool
wxMappedFDIODispatcher::RegisterFD(int fd, wxFDIOHandler *handler, int flags)
{
    // notice that it's not an error to register a handler for the same fd
    // twice as it can be done with different flags -- but it is an error to
    // register different handlers
    m_handlers.find(fd);
    m_handlers[fd] = wxFDIOHandlerEntry(handler, flags);

    return true;
}

bool wxMappedFDIODispatcher::UnregisterFD(int fd)
{
    wxFDIOHandlerMap::iterator i = m_handlers.find(fd);
    if ( i == m_handlers.end() )
      return false;

    m_handlers.erase(i);

    return true;
}

// ----------------------------------------------------------------------------
// wxSelectDispatcherModule
// ----------------------------------------------------------------------------

class wxFDIODispatcherModule : public wxModule
{
public:
    virtual bool OnInit() { return true; }
    virtual void OnExit() { wxDELETE(gs_dispatcher); }

private:
    DECLARE_DYNAMIC_CLASS(wxFDIODispatcherModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxFDIODispatcherModule, wxModule)
