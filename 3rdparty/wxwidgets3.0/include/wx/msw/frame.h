/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/frame.h
// Purpose:     wxFrame class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FRAME_H_
#define _WX_FRAME_H_

class WXDLLIMPEXP_CORE wxFrame : public wxFrameBase
{
public:
    // construction
    wxFrame() { Init(); }
    wxFrame(wxWindow *parent,
            wxWindowID id,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxDEFAULT_FRAME_STYLE,
            const wxString& name = wxFrameNameStr)
    {
        Init();

        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual ~wxFrame();

    // implement base class pure virtuals
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL);

    // implementation only from now on
    // -------------------------------

    // event handlers
    void OnSysColourChanged(wxSysColourChangedEvent& event);

    // event handlers
    bool HandleSize(int x, int y, WXUINT flag);
    bool HandleCommand(WXWORD id, WXWORD cmd, WXHWND control);

    // override the base class function to handle iconized/maximized frames
    virtual void SendSizeEvent(int flags = 0);

    virtual wxPoint GetClientAreaOrigin() const;

    // override base class version to add menu bar accel processing
    virtual bool MSWTranslateMessage(WXMSG *msg)
    {
        return MSWDoTranslateMessage(this, msg);
    }

    // window proc for the frames
    virtual WXLRESULT MSWWindowProc(WXUINT message,
                                    WXWPARAM wParam,
                                    WXLPARAM lParam);

protected:
    // common part of all ctors
    void Init();

    // override base class virtuals
    virtual void DoGetClientSize(int *width, int *height) const;
    virtual void DoSetClientSize(int width, int height);

    // propagate our state change to all child frames
    void IconizeChildFrames(bool bIconize);

    // the real implementation of MSWTranslateMessage(), also used by
    // wxMDIChildFrame
    bool MSWDoTranslateMessage(wxFrame *frame, WXMSG *msg);

    virtual bool IsMDIChild() const { return false; }

    // get default (wxWidgets) icon for the frame
    virtual WXHICON GetDefaultIcon() const;

private:
    // used by IconizeChildFrames(), see comments there
    bool m_wasMinimized;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxFrame)
};

#endif
    // _WX_FRAME_H_
