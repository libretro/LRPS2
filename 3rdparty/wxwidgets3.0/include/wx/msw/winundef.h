/////////////////////////////////////////////////////////////////////////////
// Name:        winundef.h
// Purpose:     undefine the common symbols #define'd by <windows.h>
// Author:      Vadim Zeitlin
// Modified by:
// Created:     16.05.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

/* THIS SHOULD NOT BE USED since you might include it once e.g. in window.h,
 * then again _AFTER_ you've included windows.h, in which case it won't work
 * a 2nd time -- JACS
#ifndef _WX_WINUNDEF_H_
#define _WX_WINUNDEF_H_
 */

// ----------------------------------------------------------------------------
// windows.h #defines the following identifiers which are also used in wxWin so
// we replace these symbols with the corresponding inline functions and
// undefine the macro.
//
// This looks quite ugly here but allows us to write clear (and correct!) code
// elsewhere because the functions, unlike the macros, respect the scope.
// ----------------------------------------------------------------------------

// GetClassInfo

#ifdef GetClassInfo
   #undef GetClassInfo
   #ifdef _UNICODE
   inline BOOL GetClassInfo(HINSTANCE h, LPCWSTR name, LPWNDCLASSW winclass)
   {
      return GetClassInfoW(h, name, winclass);
   }
   #else
   inline BOOL GetClassInfo(HINSTANCE h, LPCSTR name, LPWNDCLASSA winclass)
   {
      return GetClassInfoA(h, name, winclass);
   }
   #endif
#endif

#ifdef Yield
    #undef Yield
#endif
