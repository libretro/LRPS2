/////////////////////////////////////////////////////////////////////////////
// Name:        wx/stopwatch.h
// Purpose:     wxStopWatch and global time-related functions
// Author:      Julian Smart (wxTimer), Sylvain Bougnoux (wxStopWatch),
//              Vadim Zeitlin (time functions, current wxStopWatch)
// Created:     26.06.03 (extracted from wx/timer.h)
// Copyright:   (c) 1998-2003 Julian Smart, Sylvain Bougnoux
//              (c) 2011 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STOPWATCH_H_
#define _WX_STOPWATCH_H_

#include "wx/defs.h"
#include "wx/longlong.h"

// Time-related functions are also available via this header for compatibility
// but you should include wx/time.h directly if you need only them and not
// wxStopWatch itself.
#include "wx/time.h"

#if wxUSE_LONGLONG && WXWIN_COMPATIBILITY_2_6

    // Starts a global timer
    // -- DEPRECATED: use wxStopWatch instead
    wxDEPRECATED( void WXDLLIMPEXP_BASE wxStartTimer() );

    // Gets elapsed milliseconds since last wxStartTimer or wxGetElapsedTime
    // -- DEPRECATED: use wxStopWatch instead
    wxDEPRECATED( long WXDLLIMPEXP_BASE wxGetElapsedTime(bool resetTimer = true) );

#endif // wxUSE_LONGLONG && WXWIN_COMPATIBILITY_2_6

#endif // _WX_STOPWATCH_H_
