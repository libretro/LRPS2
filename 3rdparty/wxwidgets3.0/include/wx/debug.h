/////////////////////////////////////////////////////////////////////////////
// Name:        wx/debug.h
// Purpose:     Misc debug functions and macros
// Author:      Vadim Zeitlin
// Created:     29/01/98
// Copyright:   (c) 1998-2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DEBUG_H_
#define _WX_DEBUG_H_

#include  <assert.h>

#include <limits.h>          // for CHAR_BIT used below

#include "wx/chartype.h"     // for __TFILE__ and wxChar
#include "wx/cpp.h"          // for __WXFUNCTION__
#include "wx/dlimpexp.h"     // for WXDLLIMPEXP_FWD_BASE

class WXDLLIMPEXP_FWD_BASE wxString;
class WXDLLIMPEXP_FWD_BASE wxCStrData;

// ----------------------------------------------------------------------------
// Defines controlling the debugging macros
// ----------------------------------------------------------------------------

/*
    wxWidgets can be built with several different levels of debug support
    specified by the value of wxDEBUG_LEVEL constant:

    0:  No assertion macros at all, this should only be used when optimizing
        for resource-constrained systems (typically embedded ones).
    1:  Default level, most of the assertions are enabled.
    2:  Maximal (at least for now): asserts which are "expensive"
        (performance-wise) or only make sense for finding errors in wxWidgets
        itself, as opposed to bugs in applications using it, are also enabled.
 */

// ----------------------------------------------------------------------------
// Handling assertion failures
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// other miscellaneous debugger-related functions
// ----------------------------------------------------------------------------

// Use of wxFalse instead of false suppresses compiler warnings about testing
// constant expression
extern WXDLLIMPEXP_DATA_BASE(const bool) wxFalse;

// This is similar to WXUNUSED() and useful for parameters which are only used
// in assertions.
    #define WXUNUSED_UNLESS_DEBUG(param)  WXUNUSED(param)


#endif // _WX_DEBUG_H_
