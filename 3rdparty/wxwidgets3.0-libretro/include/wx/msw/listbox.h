/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/listbox.h
// Purpose:     wxListBox class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LISTBOX_H_
#define _WX_LISTBOX_H_

#if wxUSE_LISTBOX

// ----------------------------------------------------------------------------
// simple types
// ----------------------------------------------------------------------------

// forward decl for GetSelections()
class WXDLLIMPEXP_FWD_BASE wxArrayInt;

// ----------------------------------------------------------------------------
// List box control
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxListBox : public wxListBoxBase
{
public:
    // ctors and such
    wxListBox() { Init(); }
    wxListBox(wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr)
    {
        Init();

        Create(parent, id, pos, size, n, choices, style, validator, name);
    }
    wxListBox(wxWindow *parent, wxWindowID id,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr)
    {
        Init();

        Create(parent, id, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = NULL,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);
    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);

    virtual ~wxListBox();

    virtual unsigned int GetCount() const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& s);
    virtual int FindString(const wxString& s, bool bCase = false) const;

    virtual bool IsSelected(int n) const;
    virtual int GetSelection() const;
    virtual int GetSelections(wxArrayInt& aSelections) const;

    // return the index of the item at this position or wxNOT_FOUND
    int HitTest(const wxPoint& pt) const { return DoHitTestList(pt); }
    int HitTest(wxCoord x, wxCoord y) const { return DoHitTestList(wxPoint(x, y)); }

    // Windows-specific code to update the horizontal extent of the listbox, if
    // necessary. If s is non-empty, the horizontal extent is increased to the
    // length of this string if it's currently too short, otherwise the maximum
    // extent of all strings is used. In any case calls InvalidateBestSize()
    virtual void SetHorizontalExtent(const wxString& s = wxEmptyString);

    // Windows callbacks
    bool MSWCommand(WXUINT param, WXWORD id);
    WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;

    // under XP when using "transition effect for menus and tooltips" if we
    // return true for WM_PRINTCLIENT here then it causes noticable slowdown
    virtual bool MSWShouldPropagatePrintChild()
    {
        return false;
    }

    virtual wxVisualAttributes GetDefaultAttributes() const
    {
        return GetClassDefaultAttributes(GetWindowVariant());
    }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL)
    {
        return GetCompositeControlsDefaultAttributes(variant);
    }

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

    virtual void OnInternalIdle();

protected:
    virtual wxSize DoGetBestClientSize() const;

    virtual void DoClear();
    virtual void DoDeleteOneItem(unsigned int n);

    virtual void DoSetSelection(int n, bool select);

    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void DoSetFirstItem(int n);
    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;

    // this can't be called DoHitTest() because wxWindow already has this method
    virtual int DoHitTestList(const wxPoint& point) const;

    // free memory (common part of Clear() and dtor)
    void Free();

    unsigned int m_noItems;

private:
    // common part of all ctors
    void Init();

    // call this when items are added to or deleted from the listbox or an
    // items text changes
    void MSWOnItemsChanged();

    // flag indicating whether the max horizontal extent should be updated,
    // i.e. if we need to call SetHorizontalExtent() from OnInternalIdle()
    bool m_updateHorizontalExtent;


    DECLARE_DYNAMIC_CLASS_NO_COPY(wxListBox)
};

#endif // wxUSE_LISTBOX

#endif
    // _WX_LISTBOX_H_
