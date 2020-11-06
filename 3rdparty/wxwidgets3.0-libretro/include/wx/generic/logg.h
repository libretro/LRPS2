/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/logg.h
// Purpose:     Assorted wxLogXXX functions, and wxLog (sink for logs)
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef   _WX_LOGG_H_
#define   _WX_LOGG_H_

#if wxUSE_GUI

class WXDLLIMPEXP_FWD_CORE wxTextCtrl;
class WXDLLIMPEXP_FWD_CORE wxLogFrame;
class WXDLLIMPEXP_FWD_CORE wxWindow;

// ----------------------------------------------------------------------------
// the following log targets are only compiled in if the we're compiling the
// GUI part (andnot just the base one) of the library, they're implemented in
// src/generic/logg.cpp *and not src/common/log.cpp unlike all the rest)
// ----------------------------------------------------------------------------

#if wxUSE_TEXTCTRL

// log everything to a text window (GUI only of course)
class WXDLLIMPEXP_CORE wxLogTextCtrl : public wxLog
{
public:
    wxLogTextCtrl(wxTextCtrl *pTextCtrl);

protected:
    // implement sink function
    virtual void DoLogText(const wxString& msg);

private:
    // the control we use
    wxTextCtrl *m_pTextCtrl;

    wxDECLARE_NO_COPY_CLASS(wxLogTextCtrl);
};

#endif // wxUSE_TEXTCTRL

#endif // wxUSE_GUI

#endif  // _WX_LOGG_H_

