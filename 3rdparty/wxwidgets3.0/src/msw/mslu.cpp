/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/mslu.cpp
// Purpose:     Fixes for bugs in MSLU
// Author:      Vaclav Slavik
// Modified by:
// Created:     2002/02/17
// Copyright:   (c) 2002 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
    #include <dir.h>
#endif

#ifndef WX_PRECOMP
    #include "wx/utils.h"
#endif

#define wxHAS_HUGE_FILES

//------------------------------------------------------------------------
// Check for use of MSLU
//------------------------------------------------------------------------

#if wxUSE_BASE

bool WXDLLIMPEXP_BASE wxUsingUnicowsDll()
{
#if wxUSE_UNICODE_MSLU
    return (wxGetOsVersion() == wxOS_WINDOWS_9X);
#else
    return false;
#endif
}

#endif // wxUSE_BASE


#if wxUSE_UNICODE_MSLU

//------------------------------------------------------------------------
//
// NB: MSLU only covers Win32 API, it doesn't provide Unicode implementation of
//     libc functions. Unfortunately, some of MSVCRT wchar_t functions
//     (e.g. _wopen) don't work on Windows 9x, so we have to workaround it
//     by calling the char version. We still want to use wchar_t version on
//     NT/2000/XP, though, because they allow for Unicode file names.
//
//     Moreover, there are bugs in unicows.dll, of course. We have to
//     workaround them, too.
//
//------------------------------------------------------------------------

#include "wx/msw/private.h"
#include "wx/msw/mslu.h"

#include <stdio.h>
#include <io.h>
#include <sys/stat.h>

#ifdef __VISUALC__
    #include <direct.h>
#endif

// Undef redirection macros defined in wx/msw/wrapwin.h:
#undef DrawStateW
#undef GetOpenFileNameW
#undef GetSaveFileNameW

//------------------------------------------------------------------------
// Missing libc file manipulation functions in Win9x
//------------------------------------------------------------------------

#if wxUSE_BASE

WXDLLIMPEXP_BASE int wxMSLU__wrename(const wchar_t *oldname,
                                     const wchar_t *newname)
{
    if ( wxUsingUnicowsDll() )
        return rename(wxConvFile.cWX2MB(oldname), wxConvFile.cWX2MB(newname));
    else
        return _wrename(oldname, newname);
}

WXDLLIMPEXP_BASE int wxMSLU__wremove(const wchar_t *name)
{
    if ( wxUsingUnicowsDll() )
        return remove(wxConvFile.cWX2MB(name));
    else
        return _wremove(name);
}

WXDLLIMPEXP_BASE FILE* wxMSLU__wfopen(const wchar_t *name,const wchar_t* mode)
{
    if ( wxUsingUnicowsDll() )
        return fopen(wxConvFile.cWX2MB(name),wxConvFile.cWX2MB(mode));
    else
        return _wfopen(name,mode);
}

WXDLLIMPEXP_BASE FILE* wxMSLU__wfreopen(const wchar_t *name,
                                        const wchar_t* mode,
                                        FILE *stream)
{
    if ( wxUsingUnicowsDll() )
        return freopen(wxConvFile.cWX2MB(name), wxConvFile.cWX2MB(mode), stream);
    else
        return _wfreopen(name, mode, stream);
}

WXDLLIMPEXP_BASE int wxMSLU__wopen(const wchar_t *name, int flags, int mode)
{
    if ( wxUsingUnicowsDll() )
        return wxCRT_OpenA(wxConvFile.cWX2MB(name), flags, mode);
    else
        return wxCRT_OpenW(name, flags, mode);
}

WXDLLIMPEXP_BASE int wxMSLU__waccess(const wchar_t *name, int mode)
{
    if ( wxUsingUnicowsDll() )
        return wxCRT_AccessA(wxConvFile.cWX2MB(name), mode);
    else
        return wxCRT_AccessW(name, mode);
}

WXDLLIMPEXP_BASE int wxMSLU__wchmod(const wchar_t *name, int mode)
{
    if ( wxUsingUnicowsDll() )
        return wxCRT_ChmodA(wxConvFile.cWX2MB(name), mode);
    else
        return wxCRT_ChmodW(name, mode);
}

WXDLLIMPEXP_BASE int wxMSLU__wmkdir(const wchar_t *name)
{
    if ( wxUsingUnicowsDll() )
        return wxCRT_MkDirA(wxConvFile.cWX2MB(name));
    else
        return wxCRT_MkDirW(name);
}

WXDLLIMPEXP_BASE int wxMSLU__wrmdir(const wchar_t *name)
{
    if ( wxUsingUnicowsDll() )
        return wxCRT_RmDirA(wxConvFile.cWX2MB(name));
    else
        return wxCRT_RmDirW(name);
}

WXDLLIMPEXP_BASE int wxMSLU__wstat(const wchar_t *name, wxStructStat *buffer)
{
    if ( wxUsingUnicowsDll() )
        return wxCRT_StatA((const char*)wxConvFile.cWX2MB(name), buffer);
    else
        return wxCRT_StatW(name, buffer);
}

#endif // wxUSE_BASE

#endif // wxUSE_UNICODE_MSLU
