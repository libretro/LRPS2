/////////////////////////////////////////////////////////////////////////////
// Name:        wx/tracker.h
// Purpose:     Support class for object lifetime tracking (wxWeakRef<T>)
// Author:      Arne Steinarson
// Created:     28 Dec 07
// Copyright:   (c) 2007 Arne Steinarson
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TRACKER_H_
#define _WX_TRACKER_H_

#include "wx/defs.h"

// Add-on base class for a trackable object.
class WXDLLIMPEXP_BASE wxTrackable
{
protected:
    // this class is only supposed to be used as a base class but never be
    // created nor destroyed directly so all ctors and dtor are protected

    wxTrackable() { }

    // copy ctor and assignment operator intentionally do not copy m_first: the
    // objects which track the original trackable shouldn't track the new copy
    wxTrackable(const wxTrackable& WXUNUSED(other)) { }
    wxTrackable& operator=(const wxTrackable& WXUNUSED(other)) { return *this; }

    // dtor is not virtual: this class is not supposed to be used
    // polymorphically and adding a virtual table to it would add unwanted
    // overhead
    ~wxTrackable()
    {
    }
};

#endif // _WX_TRACKER_H_

