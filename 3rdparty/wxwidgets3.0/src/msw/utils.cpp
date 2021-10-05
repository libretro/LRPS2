/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/utils.cpp
// Purpose:     Various utilities
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/app.h"
#endif  //WX_PRECOMP

#include "wx/apptrait.h"
#include "wx/scopeguard.h"
#include "wx/filename.h"

#include "wx/confbase.h"        // for wxExpandEnvVars()

#include "wx/msw/private.h"     // includes <windows.h>
#include "wx/msw/missing.h"     // for CHARSET_HANGUL

#if defined(__CYGWIN__)
    //CYGWIN gives annoying warning about runtime stuff if we don't do this
#   define USE_SYS_TYPES_FD_SET
#   include <sys/types.h>
#endif

#if !defined(__GNUWIN32__) && !defined(__WXMICROWIN__) && !defined(__WXWINCE__)
    #include <direct.h>

    #include <dos.h>
#endif  //GNUWIN32

#if defined(__CYGWIN__)
    #include <sys/unistd.h>
    #include <sys/stat.h>
    #include <sys/cygwin.h> // for cygwin_conv_path()
    // and cygwin_conv_to_full_win32_path()
    #include <cygwin/version.h>
#endif  //GNUWIN32

#ifdef __BORLANDC__ // Please someone tell me which version of Borland needs
                    // this (3.1 I believe) and how to test for it.
                    // If this works for Borland 4.0 as well, then no worries.
    #include <dir.h>
#endif

// VZ: there is some code using NetXXX() functions to get the full user name:
//     I don't think it's a good idea because they don't work under Win95 and
//     seem to return the same as wxGetUserId() under NT. If you really want
//     to use them, just #define USE_NET_API
#undef USE_NET_API

#ifdef USE_NET_API
    #include <lm.h>
#endif // USE_NET_API

#if defined(__WIN32__) && !defined(__WXMICROWIN__) && !defined(__WXWINCE__)
    #ifndef __UNIX__
        #include <io.h>
    #endif

    #ifndef __GNUWIN32__
        #include <shellapi.h>
    #endif
#endif

#ifndef __WATCOMC__
    #if !(defined(_MSC_VER) && (_MSC_VER > 800))
        #include <errno.h>
    #endif
#endif

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// get host name and related
// ----------------------------------------------------------------------------

// Get user ID e.g. jacs
bool wxGetUserId(wxChar *WXUNUSED_IN_WINCE(buf),
                 int WXUNUSED_IN_WINCE(maxSize))
{
#if defined(__WXWINCE__)
    // TODO-CE
    return false;
#else
    DWORD nSize = maxSize;
    if ( ::GetUserName(buf, &nSize) == 0 )
    {
        // actually, it does happen on Win9x if the user didn't log on
        DWORD res = ::GetEnvironmentVariable(wxT("username"), buf, maxSize);
        if ( res == 0 )
        {
            // not found
            return false;
        }
    }

    return true;
#endif
}

const wxChar* wxGetHomeDir(wxString *pstr)
{
    wxString& strDir = *pstr;

    // first branch is for Cygwin
#if defined(__UNIX__) && !defined(__WINE__)
    const wxChar *szHome = wxGetenv(wxT("HOME"));
    if ( szHome == NULL ) {
      // we're homeless...
      strDir = wxT(".");
    }
    else
       strDir = szHome;

    // add a trailing slash if needed
    if ( strDir.Last() != wxT('/') )
      strDir << wxT('/');

    #ifdef __CYGWIN__
        // Cygwin returns unix type path but that does not work well
        static wxChar windowsPath[MAX_PATH];
        #if CYGWIN_VERSION_DLL_MAJOR >= 1007
            cygwin_conv_path(CCP_POSIX_TO_WIN_W, strDir.c_str(), windowsPath, MAX_PATH);
        #else
            cygwin_conv_to_full_win32_path(strDir.c_str(), windowsPath);
        #endif
        strDir = windowsPath;
    #endif
#elif defined(__WXWINCE__)
    strDir = wxT("\\");
#else
    strDir.clear();

    // If we have a valid HOME directory, as is used on many machines that
    // have unix utilities on them, we should use that.
    const wxChar *szHome = wxGetenv(wxT("HOME"));

    if ( szHome != NULL )
    {
        strDir = szHome;
    }
    else // no HOME, try HOMEDRIVE/PATH
    {
        szHome = wxGetenv(wxT("HOMEDRIVE"));
        if ( szHome != NULL )
            strDir << szHome;
        szHome = wxGetenv(wxT("HOMEPATH"));

        if ( szHome != NULL )
        {
            strDir << szHome;

            // the idea is that under NT these variables have default values
            // of "%systemdrive%:" and "\\". As we don't want to create our
            // config files in the root directory of the system drive, we will
            // create it in our program's dir. However, if the user took care
            // to set HOMEPATH to something other than "\\", we suppose that he
            // knows what he is doing and use the supplied value.
            if ( wxStrcmp(szHome, wxT("\\")) == 0 )
                strDir.clear();
        }
    }

    if ( strDir.empty() )
    {
        // If we have a valid USERPROFILE directory, as is the case in
        // Windows NT, 2000 and XP, we should use that as our home directory.
        szHome = wxGetenv(wxT("USERPROFILE"));

        if ( szHome != NULL )
            strDir = szHome;
    }

    if ( !strDir.empty() )
    {
        // sometimes the value of HOME may be "%USERPROFILE%", so reexpand the
        // value once again, it shouldn't hurt anyhow
        strDir = wxExpandEnvVars(strDir);
    }
    else // fall back to the program directory
    {
        // extract the directory component of the program file name
        wxFileName::SplitPath(wxGetFullModuleName(), &strDir, NULL, NULL);
    }
#endif  // UNIX/Win

    return strDir.c_str();
}

wxString wxGetUserHome(const wxString& user)
{
    wxString home;

    if ( user.empty() || user == wxGetUserId() )
        wxGetHomeDir(&home);

    return home;
}

// ----------------------------------------------------------------------------
// env vars
// ----------------------------------------------------------------------------

bool wxGetEnv(const wxString& WXUNUSED_IN_WINCE(var),
              wxString *WXUNUSED_IN_WINCE(value))
{
#ifdef __WXWINCE__
    // no environment variables under CE
    return false;
#else // Win32
    // first get the size of the buffer
    DWORD dwRet = ::GetEnvironmentVariable(var.t_str(), NULL, 0);
    if ( !dwRet )
    {
        // this means that there is no such variable
        return false;
    }

    if ( value )
    {
        (void)::GetEnvironmentVariable(var.t_str(),
                                       wxStringBuffer(*value, dwRet),
                                       dwRet);
    }

    return true;
#endif // WinCE/32
}

// ----------------------------------------------------------------------------
// OS version
// ----------------------------------------------------------------------------

namespace
{

// Helper function wrapping Windows GetVersionEx() which is deprecated since
// Windows 8. For now, all we do in this wrapper is to avoid the deprecation
// warnings but this is not enough as the function now actually doesn't return
// the correct value any more and we need to use VerifyVersionInfo() to perform
// binary search to find the real Windows version.
OSVERSIONINFOEX wxGetWindowsVersionInfo()
{
    OSVERSIONINFOEX info;
    wxZeroMemory(info);

#ifdef __VISUALC__
    #pragma warning(push)
    #pragma warning(disable:4996) // 'xxx': was declared deprecated
#endif

    info.dwOSVersionInfoSize = sizeof(info);
    if ( !::GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&info)) )
    {
        // This really shouldn't ever happen.
        wxFAIL_MSG( "GetVersionEx() unexpectedly failed" );
    }

#ifdef __VISUALC__
    #pragma warning(pop)
#endif

    return info;
}

// check if we're running under a server or workstation Windows system: it
// returns true or false with obvious meaning as well as -1 if the system type
// couldn't be determined
//
// this function is currently private but we may want to expose it later if
// it's really useful

int wxIsWindowsServer()
{
#ifdef VER_NT_WORKSTATION
    switch ( wxGetWindowsVersionInfo().wProductType )
    {
        case VER_NT_WORKSTATION:
            return false;

        case VER_NT_SERVER:
        case VER_NT_DOMAIN_CONTROLLER:
            return true;
    }
#endif // VER_NT_WORKSTATION

    return -1;
}

} // anonymous namespace

bool wxIsPlatform64Bit()
{
#if defined(__WIN64__)
    return true;  // 64-bit programs run only on Win64
#else
    return false;
#endif // Win64/Win32
}

// ----------------------------------------------------------------------------
// sleep functions
// ----------------------------------------------------------------------------

void wxMilliSleep(unsigned long milliseconds)
{
    ::Sleep(milliseconds);
}

void wxMicroSleep(unsigned long microseconds)
{
    wxMilliSleep(microseconds/1000);
}

void wxSleep(int nSecs)
{
    wxMilliSleep(1000*nSecs);
}
