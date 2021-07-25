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

// GetClassName

#ifdef GetClassName
   #undef GetClassName
   #ifdef _UNICODE
   inline int GetClassName(HWND h, LPWSTR classname, int maxcount)
   {
      return GetClassNameW(h, classname, maxcount);
   }
   #else
   inline int GetClassName(HWND h, LPSTR classname, int maxcount)
   {
      return GetClassNameA(h, classname, maxcount);
   }
   #endif
#endif

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

// GetMessage

#ifdef GetMessage
   #undef GetMessage
   inline int GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
   {
   #ifdef _UNICODE
      return GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
   #else
      return GetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
   #endif
   }
#endif

// LoadLibrary

#ifdef LoadLibrary
    #undef LoadLibrary
    #ifdef _UNICODE
    inline HINSTANCE LoadLibrary(LPCWSTR lpLibFileName)
    {
        return LoadLibraryW(lpLibFileName);
    }
    #else
    inline HINSTANCE LoadLibrary(LPCSTR lpLibFileName)
    {
        return LoadLibraryA(lpLibFileName);
    }
    #endif
#endif

// FindResource
#ifdef FindResource
    #undef FindResource
    #ifdef _UNICODE
    inline HRSRC FindResource(HMODULE hModule, LPCWSTR lpName, LPCWSTR lpType)
    {
        return FindResourceW(hModule, lpName, lpType);
    }
    #else
    inline HRSRC FindResource(HMODULE hModule, LPCSTR lpName, LPCSTR lpType)
    {
        return FindResourceA(hModule, lpName, lpType);
    }
    #endif
#endif

// For WINE

#if defined(GetWindowStyle)
  #undef GetWindowStyle
#endif

// For ming and cygwin

#ifdef Yield
    #undef Yield
#endif

// #endif // _WX_WINUNDEF_H_

