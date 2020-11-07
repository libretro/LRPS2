///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/preferences.h
// Purpose:     wxPreferencesEditorImpl declaration.
// Author:      Vaclav Slavik
// Created:     2013-02-19
// Copyright:   (c) 2013 Vaclav Slavik <vslavik@fastmail.fm>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_PREFERENCES_H_
#define _WX_PRIVATE_PREFERENCES_H_

#include "wx/preferences.h"

// ----------------------------------------------------------------------------
// wxPreferencesEditorImpl: defines wxPreferencesEditor implementation.
// ----------------------------------------------------------------------------

class wxPreferencesEditorImpl
{
public:
    // This is implemented in a platform-specific way.
    static wxPreferencesEditorImpl* Create(const wxString& title);

    // These methods simply mirror the public wxPreferencesEditor ones.
    virtual void AddPage(wxPreferencesPage* page) = 0;
    virtual void Show(wxWindow* parent) = 0;
    virtual void Dismiss() = 0;

    virtual ~wxPreferencesEditorImpl() {}

protected:
    wxPreferencesEditorImpl() {}
};

#endif // _WX_PRIVATE_PREFERENCES_H_
