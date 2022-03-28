///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/fd.h
// Purpose:     private stuff for working with file descriptors
// Author:      Vadim Zeitlin
// Created:     2008-11-23 (moved from wx/unix/private.h)
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_FD_H_
#define _WX_PRIVATE_FD_H_

#define wxFD_ZERO(fds) FD_ZERO(fds)
#define wxFD_SET(fd, fds) FD_SET(fd, fds)
#define wxFD_ISSET(fd, fds) FD_ISSET(fd, fds)
#define wxFD_CLR(fd, fds) FD_CLR(fd, fds)

#endif // _WX_PRIVATE_FD_H_
