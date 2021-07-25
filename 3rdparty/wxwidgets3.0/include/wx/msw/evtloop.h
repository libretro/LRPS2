///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/evtloop.h
// Purpose:     wxEventLoop class for wxMSW port
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-07-31
// Copyright:   (c) 2003-2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_EVTLOOP_H_
#define _WX_MSW_EVTLOOP_H_

#include "wx/dynarray.h"
#include "wx/msw/wrapwin.h"
#include "wx/window.h"
#include "wx/msw/evtloopconsole.h" // for wxMSWEventLoopBase

// ----------------------------------------------------------------------------
// wxEventLoop
// ----------------------------------------------------------------------------

WX_DECLARE_EXPORTED_OBJARRAY(MSG, wxMSGArray);

#endif // _WX_MSW_EVTLOOP_H_
