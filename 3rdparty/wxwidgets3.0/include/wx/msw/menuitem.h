///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/menuitem.h
// Purpose:     wxMenuItem class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     11.11.97
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef   _MENUITEM_H
#define   _MENUITEM_H

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// wxMenuItem: an item in the menu, optionally implements owner-drawn behaviour
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMenuItem : public wxMenuItemBase
{
public:
    // ctor & dtor
    wxMenuItem(wxMenu *parentMenu = NULL,
               int id = wxID_SEPARATOR,
               const wxString& name = wxEmptyString,
               const wxString& help = wxEmptyString,
               wxItemKind kind = wxITEM_NORMAL,
               wxMenu *subMenu = NULL);
    virtual ~wxMenuItem();

    // override base class virtuals
    virtual void SetItemLabel(const wxString& strName);

    virtual void Enable(bool bDoEnable = true);
    virtual void Check(bool bDoCheck = true);
    virtual bool IsChecked() const;

    // unfortunately needed to resolve ambiguity between
    // wxMenuItemBase::IsCheckable() and wxOwnerDrawn::IsCheckable()
    bool IsCheckable() const { return wxMenuItemBase::IsCheckable(); }

    // the id for a popup menu is really its menu handle (as required by
    // ::AppendMenu() API), so this function will return either the id or the
    // menu handle depending on what we are
    //
    // notice that it also returns the id as an unsigned int, as required by
    // Win32 API
    WXWPARAM GetMSWId() const;

#if WXWIN_COMPATIBILITY_2_8
    // compatibility only, don't use in new code
    wxDEPRECATED(
    wxMenuItem(wxMenu *parentMenu,
               int id,
               const wxString& text,
               const wxString& help,
               bool isCheckable,
               wxMenu *subMenu = NULL)
    );
#endif

    // Provide stubs for the public functions above to ensure that the code
    // still compiles without wxUSE_OWNER_DRAWN -- it makes sense to just drop
    // the bitmaps then instead of failing compilation.
    void SetBitmaps(const wxBitmap& WXUNUSED(bmpChecked),
                    const wxBitmap& WXUNUSED(bmpUnchecked) = wxNullBitmap) { }
    void SetBitmap(const wxBitmap& WXUNUSED(bmp),
                   bool WXUNUSED(bChecked) = true) { }
    const wxBitmap& GetBitmap() const { return wxNullBitmap; }

private:
    // common part of all ctors
    void Init();

    // Return the item position in the menu containing it.
    //
    // Returns -1 if the item is not attached to a menu or if we can't find its
    // position (which is not really supposed to ever happen).
    int MSGetMenuItemPos() const;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMenuItem)
};

#endif  //_MENUITEM_H
