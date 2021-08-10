///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/stdpbase.cpp
// Purpose:     wxStandardPathsBase methods common to all ports
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-10-19
// Copyright:   (c) 2004 Vadim Zeitlin <vadim@wxwindows.org>
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

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif //WX_PRECOMP
#include "wx/apptrait.h"

#include "wx/filename.h"
#include "wx/stdpaths.h"

// ----------------------------------------------------------------------------
// module globals
// ----------------------------------------------------------------------------

// ============================================================================
// implementation
// ============================================================================

wxStandardPathsBase::wxStandardPathsBase()
{
}

wxStandardPathsBase::~wxStandardPathsBase()
{
    // nothing to do here
}

// return the temporary directory for the current user
wxString wxStandardPathsBase::GetTempDir() const
{
    return wxFileName::GetTempDir();
}
