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

#if !defined(__WXWINCE__)
    #include  <assert.h>
#endif // systems without assert.h

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
// Debugging macros
// ----------------------------------------------------------------------------

/*
    Assertion macros: check if the condition is true and call assert handler
    (which will by default notify the user about failure) if it isn't.

    wxASSERT and wxFAIL macros as well as wxTrap() function do nothing at all
    if wxDEBUG_LEVEL is 0 however they do check their conditions at default
    debug level 1, unlike the previous wxWidgets versions.

    wxASSERT_LEVEL_2 is meant to be used for "expensive" asserts which should
    normally be disabled because they have a big impact on performance and so
    this macro only does anything if wxDEBUG_LEVEL >= 2.
 */
    #define wxTrap()

    #define wxASSERT(cond)
    #define wxASSERT_MSG(cond, msg)
    #define wxFAIL
    #define wxFAIL_MSG(msg)
    #define wxFAIL_COND_MSG(cond, msg)

    #define wxASSERT_LEVEL_2_MSG(cond, msg)
    #define wxASSERT_LEVEL_2(cond)

// This is simply a wrapper for the standard abort() which is not available
// under all platforms.
//
// It isn't really debug-related but there doesn't seem to be any better place
// for it, so declare it here and define it in appbase.cpp, together with
// wxTrap().
extern void WXDLLIMPEXP_BASE wxAbort();

/*
    wxCHECK macros always check their conditions, setting debug level to 0 only
    makes them silent in case of failure, otherwise -- including at default
    debug level 1 -- they call the assert handler if the condition is false

    They are supposed to be used only in invalid situation: for example, an
    invalid parameter (e.g. a NULL pointer) is passed to a function. Instead of
    dereferencing it and causing core dump the function might use

        wxCHECK_RET( p != NULL, "pointer can't be NULL" )
*/

// the generic macro: takes the condition to check, the statement to be executed
// in case the condition is false and the message to pass to the assert handler
#define wxCHECK2_MSG(cond, op, msg)                                       \
    if ( cond )                                                           \
    {}                                                                    \
    else                                                                  \
    {                                                                     \
        wxFAIL_COND_MSG(#cond, msg);                                      \
        op;                                                               \
    }                                                                     \
    struct wxDummyCheckStruct /* just to force a semicolon */

// check which returns with the specified return code if the condition fails
#define wxCHECK_MSG(cond, rc, msg)   wxCHECK2_MSG(cond, return rc, msg)

// check that expression is true, "return" if not (also FAILs in debug mode)
#define wxCHECK(cond, rc)            wxCHECK_MSG(cond, rc, (const char*)NULL)

// check that expression is true, perform op if not
#define wxCHECK2(cond, op)           wxCHECK2_MSG(cond, op, (const char*)NULL)

// special form of wxCHECK2: as wxCHECK, but for use in void functions
//
// NB: there is only one form (with msg parameter) and it's intentional:
//     there is no other way to tell the caller what exactly went wrong
//     from the void function (of course, the function shouldn't be void
//     to begin with...)
#define wxCHECK_RET(cond, msg)       wxCHECK2_MSG(cond, return, msg)


// ----------------------------------------------------------------------------
// other miscellaneous debugger-related functions
// ----------------------------------------------------------------------------

/*
    Return true if we're running under debugger.

    Currently only really works under Win32 and just returns false elsewhere.
 */
#if defined(__WIN32__)
    extern bool WXDLLIMPEXP_BASE wxIsDebuggerRunning();
#else // !Mac
    inline bool wxIsDebuggerRunning() { return false; }
#endif // Mac/!Mac

// Use of wxFalse instead of false suppresses compiler warnings about testing
// constant expression
extern WXDLLIMPEXP_DATA_BASE(const bool) wxFalse;

// This is similar to WXUNUSED() and useful for parameters which are only used
// in assertions.
    #define WXUNUSED_UNLESS_DEBUG(param)  WXUNUSED(param)


#endif // _WX_DEBUG_H_
