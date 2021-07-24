/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/execute.h
// Purpose:     private details of wxExecute() implementation
// Author:      Vadim Zeitlin
// Copyright:   (c) 1998 Robert Roebling, Julian Smart, Vadim Zeitlin
//              (c) 2013 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_EXECUTE_H
#define _WX_UNIX_EXECUTE_H

#include "wx/app.h"
#include "wx/hashmap.h"
#include "wx/process.h"

#if wxUSE_STREAMS
    #include "wx/unix/pipe.h"
    #include "wx/private/streamtempinput.h"
#endif

class wxEventLoopBase;

#endif // _WX_UNIX_EXECUTE_H
