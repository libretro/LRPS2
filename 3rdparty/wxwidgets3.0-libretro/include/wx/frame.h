/////////////////////////////////////////////////////////////////////////////
// Name:        wx/frame.h
// Purpose:     wxFrame class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     15.11.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FRAME_H_BASE_
#define _WX_FRAME_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/toplevel.h"      // the base class
#include "wx/statusbr.h"

// the default names for various classs
extern WXDLLIMPEXP_DATA_CORE(const char) wxStatusLineNameStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxToolBarNameStr[];

class WXDLLIMPEXP_FWD_CORE wxFrame;
class WXDLLIMPEXP_FWD_CORE wxMenuBar;
class WXDLLIMPEXP_FWD_CORE wxMenuItem;
class WXDLLIMPEXP_FWD_CORE wxStatusBar;
class WXDLLIMPEXP_FWD_CORE wxToolBar;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// wxFrame-specific (i.e. not for wxDialog) styles
//
// Also see the bit summary table in wx/toplevel.h.
#define wxFRAME_NO_TASKBAR      0x0002  // No taskbar button (MSW only)
#define wxFRAME_TOOL_WINDOW     0x0004  // No taskbar button, no system menu
#define wxFRAME_FLOAT_ON_PARENT 0x0008  // Always above its parent

// ----------------------------------------------------------------------------
// wxFrame is a top-level window with optional menubar, statusbar and toolbar
//
// For each of *bars, a frame may have several of them, but only one is
// managed by the frame, i.e. resized/moved when the frame is and whose size
// is accounted for in client size calculations - all others should be taken
// care of manually. The CreateXXXBar() functions create this, main, XXXBar,
// but the actual creation is done in OnCreateXXXBar() functions which may be
// overridden to create custom objects instead of standard ones when
// CreateXXXBar() is called.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFrameBase : public wxTopLevelWindow
{
public:
    // construction
    wxFrameBase();
    virtual ~wxFrameBase();

    wxFrame *New(wxWindow *parent,
                 wxWindowID winid,
                 const wxString& title,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxDEFAULT_FRAME_STYLE,
                 const wxString& name = wxFrameNameStr);

    // frame state
    // -----------

    // get the origin of the client area (which may be different from (0, 0)
    // if the frame has a toolbar) in client coordinates
    virtual wxPoint GetClientAreaOrigin() const;


    // menu bar functions
    // ------------------

    bool ProcessCommand(int WXUNUSED(winid)) { return false; }

    // status bar functions
    // --------------------
#if wxUSE_STATUSBAR
    // create the main status bar by calling OnCreateStatusBar()
    virtual wxStatusBar* CreateStatusBar(int number = 1,
                                         long style = wxSTB_DEFAULT_STYLE,
                                         wxWindowID winid = 0,
                                         const wxString& name = wxStatusLineNameStr);
    // return a new status bar
    virtual wxStatusBar *OnCreateStatusBar(int number,
                                           long style,
                                           wxWindowID winid,
                                           const wxString& name);
    // get the main status bar
    virtual wxStatusBar *GetStatusBar() const { return m_frameStatusBar; }

    // sets the main status bar
    virtual void SetStatusBar(wxStatusBar *statBar);

    // forward these to status bar
    virtual void SetStatusText(const wxString &text, int number = 0);
    virtual void SetStatusWidths(int n, const int widths_field[]);
    void PushStatusText(const wxString &text, int number = 0);
    void PopStatusText(int number = 0);

    // set the status bar pane the help will be shown in
    void SetStatusBarPane(int n) { m_statusBarPane = n; }
    int GetStatusBarPane() const { return m_statusBarPane; }
#endif // wxUSE_STATUSBAR

    // implementation only from now on
    // -------------------------------

    // do the UI update processing for this window
    virtual void UpdateWindowUI(long flags = wxUPDATE_UI_NONE);

    // Implement internal behaviour (menu updating on some platforms)
    virtual void OnInternalIdle();

    virtual bool IsClientAreaChild(const wxWindow *child) const
    {
        return !IsOneOfBars(child) && wxTopLevelWindow::IsClientAreaChild(child);
    }

protected:
    // the frame main menu/status/tool bars
    // ------------------------------------

    // this (non virtual!) function should be called from dtor to delete the
    // main menubar, statusbar and toolbar (if any)
    void DeleteAllBars();

    // test whether this window makes part of the frame
    virtual bool IsOneOfBars(const wxWindow *win) const;

#if wxUSE_STATUSBAR
    // override to update status bar position (or anything else) when
    // something changes
    virtual void PositionStatusBar() { }

    // show the help string for the given menu item using DoGiveHelp() if the
    // given item does have a help string (as determined by FindInMenuBar()),
    // return false if there is no help for such item
    bool ShowMenuHelp(int helpid);

    wxStatusBar *m_frameStatusBar;
#endif // wxUSE_STATUSBAR


    int m_statusBarPane;

    wxDECLARE_NO_COPY_CLASS(wxFrameBase);
};

// include the real class declaration
#if defined(__WXUNIVERSAL__) // && !defined(__WXMICROWIN__)
    #include "wx/univ/frame.h"
#else // !__WXUNIVERSAL__
    #if defined(__WXMSW__)
        #include "wx/msw/frame.h"
    #elif defined(__WXGTK20__)
        #include "wx/gtk/frame.h"
    #elif defined(__WXGTK__)
        #include "wx/gtk1/frame.h"
    #elif defined(__WXMOTIF__)
        #include "wx/motif/frame.h"
    #elif defined(__WXMAC__)
        #include "wx/osx/frame.h"
    #elif defined(__WXCOCOA__)
        #include "wx/cocoa/frame.h"
    #elif defined(__WXPM__)
        #include "wx/os2/frame.h"
    #endif
#endif

#endif
    // _WX_FRAME_H_BASE_
