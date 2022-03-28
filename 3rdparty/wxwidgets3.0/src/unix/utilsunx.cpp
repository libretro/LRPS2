/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/utilsunx.cpp
// Purpose:     generic Unix implementation of many wx functions (for wxBase)
// Author:      Vadim Zeitlin
// Copyright:   (c) 1998 Robert Roebling, Vadim Zeitlin
//              (c) 2013 Rob Bresalier, Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/utils.h"

#define USE_PUTENV (!defined(HAVE_SETENV) && defined(HAVE_PUTENV))

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/app.h"
    #include "wx/wxcrtvararg.h"
    #if USE_PUTENV
        #include "wx/module.h"
        #include "wx/hashmap.h"
    #endif
#endif

#include "wx/apptrait.h"

#include "wx/scopedptr.h"
#include "wx/thread.h"

#include "wx/wfstream.h"

#include "wx/private/selectdispatcher.h"
#include "wx/private/fdiodispatcher.h"
#include "wx/unix/pipe.h"
#include "wx/unix/private.h"

#include "wx/evtloop.h"
#include "wx/mstream.h"
#include "wx/private/fdioeventloopsourcehandler.h"

#include <pwd.h>
#include <sys/wait.h>       // waitpid()

#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif

// not only the statfs syscall is called differently depending on platform, but
// one of its incarnations, statvfs(), takes different arguments under
// different platforms and even different versions of the same system (Solaris
// 7 and 8): if you want to test for this, don't forget that the problems only
// appear if the large files support is enabled
#ifdef HAVE_STATFS
    #ifdef __BSD__
        #include <sys/param.h>
        #include <sys/mount.h>
    #else // !__BSD__
        #include <sys/vfs.h>
    #endif // __BSD__/!__BSD__

    #define wxStatfs statfs

    #ifndef HAVE_STATFS_DECL
        // some systems lack statfs() prototype in the system headers (AIX 4)
        extern "C" int statfs(const char *path, struct statfs *buf);
    #endif
#endif // HAVE_STATFS

#ifdef HAVE_STATVFS
    #include <sys/statvfs.h>

    #define wxStatfs statvfs
#endif // HAVE_STATVFS

#if defined(HAVE_STATFS) || defined(HAVE_STATVFS)
    // WX_STATFS_T is detected by configure
    #define wxStatfs_t WX_STATFS_T
#endif

#include <stdarg.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>           // nanosleep() and/or usleep()
#include <ctype.h>          // isspace()
#include <sys/time.h>       // needed for FD_SETSIZE

#ifdef HAVE_UNAME
    #include <sys/utsname.h> // for uname()
#endif // HAVE_UNAME

#ifdef HAVE_SETPRIORITY
    #include <sys/resource.h>   // for setpriority()
#endif

// ----------------------------------------------------------------------------
// conditional compilation
// ----------------------------------------------------------------------------

// many versions of Unices have this function, but it is not defined in system
// headers - please add your system here if it is the case for your OS.
// SunOS < 5.6 (i.e. Solaris < 2.6) and DG-UX are like this.
#if !defined(HAVE_USLEEP) && \
    ((defined(__SUN__) && !defined(__SunOs_5_6) && \
                         !defined(__SunOs_5_7) && !defined(__SUNPRO_CC)) || \
     defined(__osf__))
    extern "C"
    {
	    int usleep(unsigned int usec);
    };

    #define HAVE_USLEEP 1
#endif // Unices without usleep()

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// sleeping
// ----------------------------------------------------------------------------

void wxMicroSleep(unsigned long microseconds)
{
#if defined(HAVE_NANOSLEEP)
    timespec tmReq;
    tmReq.tv_sec = (time_t)(microseconds / 1000000);
    tmReq.tv_nsec = (microseconds % 1000000) * 1000;

    // we're not interested in remaining time nor in return value
    (void)nanosleep(&tmReq, NULL);
#elif defined(HAVE_USLEEP)
    // uncomment this if you feel brave or if you are sure that your version
    // of Solaris has a safe usleep() function but please notice that usleep()
    // is known to lead to crashes in MT programs in Solaris 2.[67] and is not
    // documented as MT-Safe
    #if defined(__SUN__) && wxUSE_THREADS
        #error "usleep() cannot be used in MT programs under Solaris."
    #endif // Sun

    usleep(microseconds);
#elif defined(HAVE_SLEEP)
    // under BeOS sleep() takes seconds (what about other platforms, if any?)
    sleep(microseconds * 1000000);
#else // !sleep function
    #error "usleep() or nanosleep() function required for wxMicroSleep"
#endif // sleep function
}

void wxMilliSleep(unsigned long milliseconds)
{
    wxMicroSleep(milliseconds*1000);
}

// ----------------------------------------------------------------------------
// wxShell
// ----------------------------------------------------------------------------
#undef ERROR_RETURN_CODE

// ----------------------------------------------------------------------------
// file and directory functions
// ----------------------------------------------------------------------------

const wxChar* wxGetHomeDir( wxString *home  )
{
    *home = wxGetUserHome();
    wxString tmp;
    if ( home->empty() )
        *home = wxT("/");
    return home->c_str();
}

wxString wxGetUserHome( const wxString &user )
{
    struct passwd *who = (struct passwd *) NULL;

    if ( !user )
    {
        wxChar *ptr;

        if ((ptr = wxGetenv(wxT("HOME"))) != NULL)
            return ptr;

        if ((ptr = wxGetenv(wxT("USER"))) != NULL ||
             (ptr = wxGetenv(wxT("LOGNAME"))) != NULL)
            who = getpwnam(wxSafeConvertWX2MB(ptr));

        // make sure the user exists!
        if ( !who )
            who = getpwuid(getuid());
    }
    else
      who = getpwnam (user.mb_str());

    return wxSafeConvertMB2WX(who ? who->pw_dir : 0);
}

// ----------------------------------------------------------------------------
// network and user id routines
// ----------------------------------------------------------------------------

bool wxGetUserId(wxChar *buf, int sz)
{
    struct passwd *who;

    *buf = wxT('\0');
    if ((who = getpwuid(getuid ())) != NULL)
    {
        wxStrlcpy (buf, wxSafeConvertMB2WX(who->pw_name), sz);
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// env vars
// ----------------------------------------------------------------------------

bool wxGetEnv(const wxString& var, wxString *value)
{
    // wxGetenv is defined as getenv()
    char *p = wxGetenv(var);
    if ( !p )
        return false;

    if ( value )
        *value = p;

    return true;
}
