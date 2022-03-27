/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/init.h
// Purpose:     Windows-specific wxEntry() overload
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_INIT_H_
#define _WX_MSW_INIT_H_

// ----------------------------------------------------------------------------
// Windows-specific wxEntry() overload and wxIMPLEMENT_WXWIN_MAIN definition
// ----------------------------------------------------------------------------

// we need HINSTANCE declaration to define WinMain()
#include "wx/msw/wrapwin.h"

#ifndef SW_SHOWNORMAL
    #define SW_SHOWNORMAL 1
#endif

typedef char *wxCmdLineArgType;

// Call this function to prevent wxMSW from calling SetProcessDPIAware().
// Must be called before wxEntry().
extern WXDLLIMPEXP_CORE void wxMSWDisableSettingHighDPIAware();

// Windows-only overloads of wxEntry() and wxEntryStart() which take the
// parameters passed to WinMain() instead of those passed to main()
extern WXDLLIMPEXP_CORE bool
    wxEntryStart(HINSTANCE hInstance,
                HINSTANCE hPrevInstance = NULL,
                wxCmdLineArgType pCmdLine = NULL,
                int nCmdShow = SW_SHOWNORMAL);

extern WXDLLIMPEXP_CORE int
    wxEntry(HINSTANCE hInstance,
            HINSTANCE hPrevInstance = NULL,
            wxCmdLineArgType pCmdLine = NULL,
            int nCmdShow = SW_SHOWNORMAL);

#define wxIMPLEMENT_WXWIN_MAIN_BORLAND_NONSTANDARD

#define wxIMPLEMENT_WXWIN_MAIN                                              \
    extern "C" int WINAPI WinMain(HINSTANCE hInstance,                      \
                                  HINSTANCE hPrevInstance,                  \
                                  wxCmdLineArgType WXUNUSED(lpCmdLine),     \
                                  int nCmdShow)                             \
    {                                                                       \
        /* NB: We pass NULL in place of lpCmdLine to behave the same as  */ \
        /*     Borland-specific wWinMain() above. If it becomes needed   */ \
        /*     to pass lpCmdLine to wxEntry() here, you'll have to fix   */ \
        /*     wWinMain() above too.                                     */ \
        return wxEntry(hInstance, hPrevInstance, NULL, nCmdShow);           \
    }                                                                       \
    wxIMPLEMENT_WXWIN_MAIN_BORLAND_NONSTANDARD


#endif // _WX_MSW_INIT_H_
