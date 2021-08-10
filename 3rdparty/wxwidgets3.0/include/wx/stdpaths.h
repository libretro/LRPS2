///////////////////////////////////////////////////////////////////////////////
// Name:        wx/stdpaths.h
// Purpose:     declaration of wxStandardPaths class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-10-17
// Copyright:   (c) 2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STDPATHS_H_
#define _WX_STDPATHS_H_

#include "wx/defs.h"

#include "wx/string.h"
#include "wx/filefn.h"

class WXDLLIMPEXP_FWD_BASE wxStandardPaths;

// ----------------------------------------------------------------------------
// wxStandardPaths returns the standard locations in the file system
// ----------------------------------------------------------------------------

// NB: This is always compiled in, wxUSE_STDPATHS=0 only disables native
//     wxStandardPaths class, but a minimal version is always available
class WXDLLIMPEXP_BASE wxStandardPathsBase
{
public:
    // return the temporary directory for the current user
    virtual wxString GetTempDir() const;

    // virtual dtor for the base class
    virtual ~wxStandardPathsBase();

protected:
    // Ctor is protected as this is a base class which should never be created
    // directly.
    wxStandardPathsBase();
};

// ----------------------------------------------------------------------------
// Minimal generic implementation
// ----------------------------------------------------------------------------

// NB: Note that this minimal implementation is compiled in even if
//     wxUSE_STDPATHS=0, so that our code can still use wxStandardPaths.

class WXDLLIMPEXP_BASE wxStandardPaths : public wxStandardPathsBase
{
protected:
    // Ctor is protected because wxStandardPaths::Get() should always be used
    // to access the global wxStandardPaths object of the correct type instead
    // of creating one of a possibly wrong type yourself.
    wxStandardPaths() { }
};

#endif // _WX_STDPATHS_H_

