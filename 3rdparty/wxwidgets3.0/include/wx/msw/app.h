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

class WXDLLIMPEXP_FWD_CORE wxWindow;
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

    // MSW-specific from now on
    // ------------------------

    // get the name of the registered Win32 class with the given (unique) base
    // name: this function constructs the unique class name using this name as
    // prefix, checks if the class is already registered and registers it if it
    // isn't and returns the name it was registered under (or NULL if it failed)
    //
    // the registered class will always have CS_[HV]REDRAW and CS_DBLCLKS
    // styles as well as any additional styles specified as arguments here; and
    // there will be also a companion registered class identical to this one
    // but without CS_[HV]REDRAW whose name will be the same one but with
    // GetNoRedrawClassSuffix()
    //
    // the background brush argument must be either a COLOR_XXX standard value
    // or (default) -1 meaning that the class paints its background itself
    static const wxChar *GetRegisteredClassName(const wxChar *name,
                                                int bgBrushCol = -1,
                                                int extraStyles = 0);

    // return true if this name corresponds to one of the classes we registered
    // in the previous GetRegisteredClassName() calls
    static bool IsRegisteredClassName(const wxString& name);

public:
    // unregister any window classes registered by GetRegisteredClassName()
    static void UnregisterWindowClasses();

protected:
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxApp);
    DECLARE_DYNAMIC_CLASS(wxApp)
};

#endif // _WX_APP_H_

