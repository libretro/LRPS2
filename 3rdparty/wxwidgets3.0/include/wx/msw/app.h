/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/app.h
// Purpose:     wxApp class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_APP_H_
#define _WX_APP_H_

#include "wx/event.h"
#include "wx/icon.h"

class WXDLLIMPEXP_FWD_CORE wxApp;
class WXDLLIMPEXP_FWD_CORE wxKeyEvent;

// Represents the application. Derive OnInit and declare
// a new App object to start application
class WXDLLIMPEXP_CORE wxApp : public wxAppBase
{
public:
    wxApp();
    virtual ~wxApp();

    // override base class (pure) virtuals
    virtual bool Initialize(int& argc, wxChar **argv);
    virtual void CleanUp();

    virtual void WakeUpIdle();
protected:
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxApp);
    DECLARE_DYNAMIC_CLASS(wxApp)
};

#endif // _WX_APP_H_
