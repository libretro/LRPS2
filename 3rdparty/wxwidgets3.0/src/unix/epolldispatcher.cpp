///////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/epolldispatcher.cpp
// Purpose:     implements dispatcher for epoll_wait() call
// Author:      Lukasz Michalski
// Created:     April 2007
// Copyright:   (c) 2007 Lukasz Michalski
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

#if wxUSE_EPOLL_DISPATCHER

#include "wx/unix/private/epolldispatcher.h"
#include "wx/unix/private.h"

#include "wx/time.h"
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>

#define wxEpollDispatcher_Trace wxT("epolldispatcher")

// ============================================================================
// implementation
// ============================================================================

// helper: return EPOLLxxx mask corresponding to the given flags (and also log
// debugging messages about it)
static uint32_t GetEpollMask(int flags, int fd)
{
    uint32_t ep = 0;

    if ( flags & wxFDIO_INPUT )
        ep |= EPOLLIN;

    if ( flags & wxFDIO_OUTPUT )
        ep |= EPOLLOUT;

    if ( flags & wxFDIO_EXCEPTION )
        ep |= EPOLLERR | EPOLLHUP;

    return ep;
}

// ----------------------------------------------------------------------------
// wxEpollDispatcher
// ----------------------------------------------------------------------------

/* static */
wxEpollDispatcher *wxEpollDispatcher::Create()
{
    int epollDescriptor = epoll_create(1024);
    if ( epollDescriptor == -1 )
        return NULL;
    return new wxEpollDispatcher(epollDescriptor);
}

wxEpollDispatcher::wxEpollDispatcher(int epollDescriptor)
{
    m_epollDescriptor = epollDescriptor;
}

wxEpollDispatcher::~wxEpollDispatcher()
{
    if ( close(m_epollDescriptor) != 0 ) { }
}

bool wxEpollDispatcher::RegisterFD(int fd, wxFDIOHandler* handler, int flags)
{
    epoll_event ev;
    ev.events = GetEpollMask(flags, fd);
    ev.data.ptr = handler;

    const int ret = epoll_ctl(m_epollDescriptor, EPOLL_CTL_ADD, fd, &ev);
    if ( ret != 0 )
        return false;

    return true;
}

bool wxEpollDispatcher::UnregisterFD(int fd)
{
    epoll_event ev;
    ev.events = 0;
    ev.data.ptr = NULL;

    if ( epoll_ctl(m_epollDescriptor, EPOLL_CTL_DEL, fd, &ev) != 0 ) { }
    return true;
}

int
wxEpollDispatcher::DoPoll(epoll_event *events, int numEvents, int timeout) const
{
    wxMilliClock_t timeEnd;
    if ( timeout > 0 )
        timeEnd = wxGetLocalTimeMillis();

    int rc;
    for ( ;; )
    {
        rc = epoll_wait(m_epollDescriptor, events, numEvents, timeout);
        if ( rc != -1 || errno != EINTR )
            break;

        // we got interrupted, update the timeout and restart
        if ( timeout > 0 )
        {
            timeout = wxMilliClockToLong(timeEnd - wxGetLocalTimeMillis());
            if ( timeout < 0 )
                return 0;
        }
    }

    return rc;
}

bool wxEpollDispatcher::HasPending() const
{
    epoll_event event;

    // NB: it's not really clear if epoll_wait() can return a number greater
    //     than the number of events passed to it but just in case it can, use
    //     >= instead of == here, see #10397
    return DoPoll(&event, 1, 0) >= 1;
}

int wxEpollDispatcher::Dispatch(int timeout)
{
    epoll_event events[16];

    const int rc = DoPoll(events, WXSIZEOF(events), timeout);

    if ( rc == -1 )
        return -1;

    int numEvents = 0;
    for ( epoll_event *p = events; p < events + rc; p++ )
    {
        wxFDIOHandler * const handler = (wxFDIOHandler *)(p->data.ptr);
        if ( !handler )
            continue;

        // note that for compatibility with wxSelectDispatcher we call
        // OnReadWaiting() on EPOLLHUP as this is what epoll_wait() returns
        // when the write end of a pipe is closed while with select() the
        // remaining pipe end becomes ready for reading when this happens
        if ( p->events & (EPOLLIN | EPOLLHUP) )
            handler->OnReadWaiting();
        else if ( p->events & EPOLLOUT )
            handler->OnWriteWaiting();
        else if ( p->events & EPOLLERR )
            handler->OnExceptionWaiting();
        else
            continue;

        numEvents++;
    }

    return numEvents;
}

#endif // wxUSE_EPOLL_DISPATCHER
