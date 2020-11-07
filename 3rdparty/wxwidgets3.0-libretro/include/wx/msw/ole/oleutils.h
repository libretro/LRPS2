///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/ole/oleutils.h
// Purpose:     OLE helper routines, OLE debugging support &c
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.02.1998
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef   _WX_OLEUTILS_H
#define   _WX_OLEUTILS_H

#include "wx/defs.h"

// ----------------------------------------------------------------------------
// stub functions to avoid #if wxUSE_OLE in the main code
// ----------------------------------------------------------------------------

inline bool wxOleInitialize() { return false; }
inline void wxOleUninitialize() { }

// RAII class initializing OLE in its ctor and undoing it in its dtor.
class wxOleInitializer
{
public:
    wxOleInitializer()
        : m_ok(wxOleInitialize())
    {
    }

    bool IsOk() const
    {
        return m_ok;
    }

    ~wxOleInitializer()
    {
        if ( m_ok )
            wxOleUninitialize();
    }

private:
    const bool m_ok;

    wxDECLARE_NO_COPY_CLASS(wxOleInitializer);
};

#endif  //_WX_OLEUTILS_H
