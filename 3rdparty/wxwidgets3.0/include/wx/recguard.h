///////////////////////////////////////////////////////////////////////////////
// Name:        wx/recguard.h
// Purpose:     declaration and implementation of wxRecursionGuard class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.08.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RECGUARD_H_
#define _WX_RECGUARD_H_

#include "wx/defs.h"

// ----------------------------------------------------------------------------
// wxRecursionGuardFlag is used with wxRecursionGuard
// ----------------------------------------------------------------------------

typedef int wxRecursionGuardFlag;

// ----------------------------------------------------------------------------
// wxRecursionGuard is the simplest way to protect a function from reentrancy
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxRecursionGuard
{
public:
    wxRecursionGuard(wxRecursionGuardFlag& flag)
        : m_flag(flag)
    {
    }

    ~wxRecursionGuard()
    {
        wxASSERT_MSG( m_flag > 0, wxT("unbalanced wxRecursionGuards!?") );

        m_flag--;
    }

private:
    wxRecursionGuardFlag& m_flag;
};

#endif // _WX_RECGUARD_H_

