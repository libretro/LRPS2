/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/private.h
// Purpose:     Private declarations: as this header is only included by
//              wxWidgets itself, it may contain identifiers which don't start
//              with "wx".
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_H_
#define _WX_PRIVATE_H_

#include "wx/msw/wrapwin.h"

class WXDLLIMPEXP_FWD_CORE wxFont;

// ---------------------------------------------------------------------------
// private constants
// ---------------------------------------------------------------------------

// 260 was taken from windef.h
#ifndef MAX_PATH
    #define MAX_PATH  260
#endif

// ---------------------------------------------------------------------------
// global data
// ---------------------------------------------------------------------------

extern WXDLLIMPEXP_DATA_BASE(HINSTANCE) wxhInstance;

extern "C"
{
    WXDLLIMPEXP_BASE HINSTANCE wxGetInstance();
}

WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);

// ---------------------------------------------------------------------------
// define things missing from some compilers' headers
// ---------------------------------------------------------------------------

#if (defined(__GNUWIN32__) && !wxUSE_NORLANDER_HEADERS)
#ifndef ZeroMemory
    inline void ZeroMemory(void *buf, size_t len) { memset(buf, 0, len); }
#endif
#endif // old mingw32

// this defines a CASTWNDPROC macro which casts a pointer to the type of a
// window proc
#if defined(STRICT) || defined(__GNUC__)
    typedef WNDPROC WndProcCast;
#else
    typedef FARPROC WndProcCast;
#endif


#define CASTWNDPROC (WndProcCast)



// ---------------------------------------------------------------------------
// some stuff for old Windows versions (FIXME: what does it do here??)
// ---------------------------------------------------------------------------

#if !defined(APIENTRY)  // NT defines APIENTRY, 3.x not
    #define APIENTRY FAR PASCAL
#endif

#ifdef __WIN32__
    #define _EXPORT
#else
    #define _EXPORT _export
#endif

#ifndef __WIN32__
    typedef signed short int SHORT;
#endif

#if !defined(__WIN32__)  // 3.x uses FARPROC for dialogs
#ifndef STRICT
    #define DLGPROC FARPROC
#endif
#endif

/*
 * Decide what window classes we're going to use
 * for this combination of CTl3D/FAFA settings
 */

#define STATIC_CLASS     wxT("STATIC")
#define STATIC_FLAGS     (SS_LEFT|WS_CHILD|WS_VISIBLE)
#define CHECK_CLASS      wxT("BUTTON")
#define CHECK_FLAGS      (BS_AUTOCHECKBOX|WS_TABSTOP|WS_CHILD)
#define CHECK_IS_FAFA    FALSE
#define RADIO_CLASS      wxT("BUTTON")
#define RADIO_FLAGS      (BS_AUTORADIOBUTTON|WS_CHILD|WS_VISIBLE)
#define RADIO_SIZE       20
#define RADIO_IS_FAFA    FALSE
#define PURE_WINDOWS
#define GROUP_CLASS      wxT("BUTTON")
#define GROUP_FLAGS      (BS_GROUPBOX|WS_CHILD|WS_VISIBLE)

/*
#define BITCHECK_FLAGS   (FB_BITMAP|FC_BUTTONDRAW|FC_DEFAULT|WS_VISIBLE)
#define BITRADIO_FLAGS   (FC_BUTTONDRAW|FB_BITMAP|FC_RADIO|WS_CHILD|WS_VISIBLE)
*/

// ---------------------------------------------------------------------------
// misc macros
// ---------------------------------------------------------------------------

#define MEANING_CHARACTER '0'
#define DEFAULT_ITEM_WIDTH  100
#define DEFAULT_ITEM_HEIGHT 80

// Scale font to get edit control height
//#define EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy)    (3*(cy)/2)
#define EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy)    (cy+8)

// Generic subclass proc, for panel item moving/sizing and intercept
// EDIT control VK_RETURN messages
extern LONG APIENTRY _EXPORT
  wxSubclassedGenericControlProc(WXHWND hWnd, WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

// ---------------------------------------------------------------------------
// useful macros and functions
// ---------------------------------------------------------------------------

// a wrapper macro for ZeroMemory()
#if defined(__WIN32__)
#define wxZeroMemory(obj)   ::ZeroMemory(&obj, sizeof(obj))
#else
#define wxZeroMemory(obj)   memset((void*) & obj, 0, sizeof(obj))
#endif

// This one is a macro so that it can be tested with #ifdef, it will be
// undefined if it cannot be implemented for a given compiler.
// Vc++, bcc, dmc, ow, mingw akk have _get_osfhandle() and Cygwin has
// get_osfhandle. Others are currently unknown, e.g. Salford, Intel, Visual
// Age.
#if defined(__CYGWIN__)
    #define wxGetOSFHandle(fd) ((HANDLE)get_osfhandle(fd))
#elif defined(__VISUALC__) \
   || defined(__MINGW32__)
    #define wxGetOSFHandle(fd) ((HANDLE)_get_osfhandle(fd))
    #define wxOpenOSFHandle(h, flags) (_open_osfhandle(wxPtrToUInt(h), flags))

    wxDECL_FOR_STRICT_MINGW32(FILE*, _fdopen, (int, const char*))
    #define wx_fdopen _fdopen
#endif

// close the handle in the class dtor
class AutoHANDLE
{
public:
    wxEXPLICIT AutoHANDLE(HANDLE handle) : m_handle(handle) { }

    bool IsOk() const { return m_handle != INVALID_HANDLE_VALUE; }
    operator HANDLE() const { return m_handle; }

    ~AutoHANDLE() { if ( IsOk() ) ::CloseHandle(m_handle); }

protected:
    HANDLE m_handle;
};

// a template to make initializing Windows styructs less painful: it zeroes all
// the struct fields and also sets cbSize member to the correct value (and so
// can be only used with structures which have this member...)
template <class T>
struct WinStruct : public T
{
    WinStruct()
    {
        ::ZeroMemory(this, sizeof(T));

        // explicit qualification is required here for this to be valid C++
        this->cbSize = sizeof(T);
    }
};


// Macros for converting wxString to the type expected by API functions.
//
// Normally it is enough to just use wxString::t_str() which is implicitly
// convertible to LPCTSTR, but in some cases an explicit conversion is required.
//
// In such cases wxMSW_CONV_LPCTSTR() should be used. But if an API function
// takes a non-const pointer, wxMSW_CONV_LPTSTR() which casts away the
// constness (but doesn't make it possible to really modify the returned
// pointer, of course) should be used. And if a string is passed as LPARAM, use
// wxMSW_CONV_LPARAM() which does the required ugly reinterpret_cast<> too.
#define wxMSW_CONV_LPCTSTR(s) static_cast<const wxChar *>((s).t_str())
#define wxMSW_CONV_LPTSTR(s) const_cast<wxChar *>(wxMSW_CONV_LPCTSTR(s))
#define wxMSW_CONV_LPARAM(s) reinterpret_cast<LPARAM>(wxMSW_CONV_LPCTSTR(s))

// ---------------------------------------------------------------------------
// global functions
// ---------------------------------------------------------------------------

// return the full path of the given module
inline wxString wxGetFullModuleName(HMODULE hmod)
{
    wxString fullname;
    if ( !::GetModuleFileName
            (
                hmod,
                wxStringBuffer(fullname, MAX_PATH),
                MAX_PATH
            ) ) { }

    return fullname;
}

// return the full path of the program file
inline wxString wxGetFullModuleName()
{
    return wxGetFullModuleName((HMODULE)wxGetInstance());
}

// return the run-time version of the OS in a format similar to
// WINVER/_WIN32_WINNT compile-time macros:
//
//      0x0300      Windows NT 3.51
//      0x0400      Windows 95, NT4
//      0x0410      Windows 98
//      0x0500      Windows ME, 2000
//      0x0501      Windows XP, 2003
//      0x0502      Windows XP SP2, 2003 SP1
//      0x0600      Windows Vista, 2008
//      0x0601      Windows 7
//      0x0602      Windows 8 (currently also returned for 8.1 if program does not have a manifest indicating 8.1 support)
//      0x0603      Windows 8.1 (currently only returned for 8.1 if program has a manifest indicating 8.1 support)
//      0x1000      Windows 10 (currently only returned for 10 if program has a manifest indicating 10 support)
//
// for the other Windows versions 0 is currently returned
enum wxWinVersion
{
    wxWinVersion_Unknown = 0,

    wxWinVersion_3 = 0x0300,
    wxWinVersion_NT3 = wxWinVersion_3,

    wxWinVersion_4 = 0x0400,
    wxWinVersion_95 = wxWinVersion_4,
    wxWinVersion_NT4 = wxWinVersion_4,
    wxWinVersion_98 = 0x0410,

    wxWinVersion_5 = 0x0500,
    wxWinVersion_ME = wxWinVersion_5,
    wxWinVersion_NT5 = wxWinVersion_5,
    wxWinVersion_2000 = wxWinVersion_5,
    wxWinVersion_XP = 0x0501,
    wxWinVersion_2003 = 0x0501,
    wxWinVersion_XP_SP2 = 0x0502,
    wxWinVersion_2003_SP1 = 0x0502,

    wxWinVersion_6 = 0x0600,
    wxWinVersion_Vista = wxWinVersion_6,
    wxWinVersion_NT6 = wxWinVersion_6,

    wxWinVersion_7 = 0x601,

    wxWinVersion_8 = 0x602,
    wxWinVersion_8_1 = 0x603,

    wxWinVersion_10 = 0x1000
};

#endif // _WX_PRIVATE_H_
