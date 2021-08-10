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

// SGI signal.h defines signal handler arguments differently depending on
// whether _LANGUAGE_C_PLUS_PLUS is set or not - do set it
#if defined(__SGI__) && !defined(_LANGUAGE_C_PLUS_PLUS)
    #define _LANGUAGE_C_PLUS_PLUS 1
#endif // SGI hack

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

// Used by wxGetFreeMemory().
#ifdef __SGI__
    #include <sys/sysmp.h>
    #include <sys/sysinfo.h>   // for SAGET and MINFO structures
#endif

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
     defined(__osf__) || defined(__EMX__))
    extern "C"
    {
        #ifdef __EMX__
            /* I copied this from the XFree86 diffs. AV. */
            #define INCL_DOSPROCESS
            #include <os2.h>
            inline void usleep(unsigned long delay)
            {
                DosSleep(delay ? (delay/1000l) : 1l);
            }
        #else // Unix
            int usleep(unsigned int usec);
        #endif // __EMX__/Unix
    };

    #define HAVE_USLEEP 1
#endif // Unices without usleep()

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// sleeping
// ----------------------------------------------------------------------------

void wxSleep(int nSecs)
{
    sleep(nSecs);
}

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

namespace
{

// helper class for storing arguments as char** array suitable for passing to
// execvp(), whatever form they were passed to us
class ArgsArray
{
public:
    ArgsArray(const wxArrayString& args)
    {
        Init(args.size());

        for ( int i = 0; i < m_argc; i++ )
        {
            m_argv[i] = wxStrdup(args[i]);
        }
    }

#if wxUSE_UNICODE
    ArgsArray(wchar_t **wargv)
    {
        int argc = 0;
        while ( wargv[argc] )
            argc++;

        Init(argc);

        for ( int i = 0; i < m_argc; i++ )
        {
            m_argv[i] = wxSafeConvertWX2MB(wargv[i]).release();
        }
    }
#endif // wxUSE_UNICODE

    ~ArgsArray()
    {
        for ( int i = 0; i < m_argc; i++ )
        {
            free(m_argv[i]);
        }

        delete [] m_argv;
    }

    operator char**() const { return m_argv; }

private:
    void Init(int argc)
    {
        m_argc = argc;
        m_argv = new char *[m_argc + 1];
        m_argv[m_argc] = NULL;
    }

    int m_argc;
    char **m_argv;

    wxDECLARE_NO_COPY_CLASS(ArgsArray);
};

} // anonymous namespace

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
#ifdef __VMS
    tmp = *home;
    if ( tmp.Last() != wxT(']'))
        if ( tmp.Last() != wxT('/')) *home << wxT('/');
#endif
    return home->c_str();
}

wxString wxGetUserHome( const wxString &user )
{
    struct passwd *who = (struct passwd *) NULL;

    if ( !user )
    {
        wxChar *ptr;

        if ((ptr = wxGetenv(wxT("HOME"))) != NULL)
        {
            return ptr;
        }

        if ((ptr = wxGetenv(wxT("USER"))) != NULL ||
             (ptr = wxGetenv(wxT("LOGNAME"))) != NULL)
        {
            who = getpwnam(wxSafeConvertWX2MB(ptr));
        }

        // make sure the user exists!
        if ( !who )
        {
            who = getpwuid(getuid());
        }
    }
    else
    {
      who = getpwnam (user.mb_str());
    }

    return wxSafeConvertMB2WX(who ? who->pw_dir : 0);
}

// ----------------------------------------------------------------------------
// network and user id routines
// ----------------------------------------------------------------------------

// Private utility function which returns output of the given command, removing
// the trailing newline.
//
// Note that by default use Latin-1 just to ensure that we never fail, but if
// the encoding is known (e.g. UTF-8 for lsb_release), it should be explicitly
// used instead.
static wxString
wxGetCommandOutput(const wxString &cmd, wxMBConv& conv = wxConvISO8859_1)
{
    // Suppress stderr from the shell to avoid outputting errors if the command
    // doesn't exist.
    FILE *f = popen((cmd + " 2>/dev/null").ToAscii(), "r");
    if ( !f )
        return wxString();

    wxString s;
    char buf[256];
    while ( !feof(f) )
    {
        if ( !fgets(buf, sizeof(buf), f) )
            break;

        s += wxString(buf, conv);
    }

    pclose(f);

    if ( !s.empty() && s.Last() == wxT('\n') )
        s.RemoveLast();

    return s;
}

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

bool wxIsPlatform64Bit()
{
    const wxString machine = wxGetCommandOutput(wxT("uname -m"));

    // the test for "64" is obviously not 100% reliable but seems to work fine
    // in practice
    return machine.Contains(wxT("64")) ||
                machine.Contains(wxT("alpha"));
}

#ifdef __LINUX__

static bool
wxGetValueFromLSBRelease(wxString arg, const wxString& lhs, wxString* rhs)
{
    // lsb_release seems to just read a global file which is always in UTF-8
    // and hence its output is always in UTF-8 as well, regardless of the
    // locale currently configured by our environment.
    return wxGetCommandOutput(wxS("lsb_release ") + arg, wxConvUTF8)
                .StartsWith(lhs, rhs);
}

wxLinuxDistributionInfo wxGetLinuxDistributionInfo()
{
    wxLinuxDistributionInfo ret;

    if ( !wxGetValueFromLSBRelease(wxS("--id"), wxS("Distributor ID:\t"),
                                   &ret.Id) )
    {
        // Don't bother to continue, lsb_release is probably not available.
        return ret;
    }

    wxGetValueFromLSBRelease(wxS("--description"), wxS("Description:\t"),
                             &ret.Description);
    wxGetValueFromLSBRelease(wxS("--release"), wxS("Release:\t"),
                             &ret.Release);
    wxGetValueFromLSBRelease(wxS("--codename"), wxS("Codename:\t"),
                             &ret.CodeName);

    return ret;
}
#endif // __LINUX__

// these functions are in src/osx/utilsexc_base.cpp for wxMac
#ifndef __DARWIN__

wxOperatingSystemId wxGetOsVersion(int *verMaj, int *verMin)
{
    // get OS version
    int major, minor;
    wxString release = wxGetCommandOutput(wxT("uname -r"));
    if ( release.empty() ||
         wxSscanf(release.c_str(), wxT("%d.%d"), &major, &minor) != 2 )
    {
        // failed to get version string or unrecognized format
        major =
        minor = -1;
    }

    if ( verMaj )
        *verMaj = major;
    if ( verMin )
        *verMin = minor;

    // try to understand which OS are we running
    wxString kernel = wxGetCommandOutput(wxT("uname -s"));
    if ( kernel.empty() )
        kernel = wxGetCommandOutput(wxT("uname -o"));

    if ( kernel.empty() )
        return wxOS_UNKNOWN;

    return wxPlatformInfo::GetOperatingSystemId(kernel);
}

wxString wxGetOsDescription()
{
    return wxGetCommandOutput(wxT("uname -s -r -m"));
}

#endif // !__DARWIN__

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
    {
        *value = p;
    }

    return true;
}

namespace
{

// Helper function that checks whether the child with the given PID has exited
// and fills the provided parameter with its return code if it did.
bool CheckForChildExit(int pid, int* exitcodeOut)
{
    wxASSERT_MSG( pid > 0, "invalid PID" );

    int status, rc;

    // loop while we're getting EINTR
    for ( ;; )
    {
        rc = waitpid(pid, &status, WNOHANG);

        if ( rc != -1 || errno != EINTR )
            break;
    }

    switch ( rc )
    {
        case 0:
            // No error but the child is still running.
            return false;

        case -1:
            // Checking child status failed. Invalid PID?
            return false;

        default:
            // Child did terminate.
            wxASSERT_MSG( rc == pid, "unexpected waitpid() return value" );

            // notice that the caller expects the exit code to be signed, e.g. -1
            // instead of 255 so don't assign WEXITSTATUS() to an int
            signed char exitcode;
            if ( WIFEXITED(status) )
                exitcode = WEXITSTATUS(status);
            else if ( WIFSIGNALED(status) )
                exitcode = -WTERMSIG(status);
            else
                exitcode = -1;

            if ( exitcodeOut )
                *exitcodeOut = exitcode;

            return true;
    }
}

} // anonymous namespace
