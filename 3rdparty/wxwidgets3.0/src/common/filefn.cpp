/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/filefn.cpp
// Purpose:     File- and directory-related functions
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
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

#include "wx/filefn.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/crt.h"
#endif

#include "wx/dynarray.h"
#include "wx/file.h"
#include "wx/filename.h"
#include "wx/dir.h"

#include "wx/tokenzr.h"

// there are just too many of those...
#ifdef __VISUALC__
    #pragma warning(disable:4706)   // assignment within conditional expression
#endif // VC++

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !(defined(_MSC_VER) && (_MSC_VER > 800))
#include <errno.h>
#endif

#if defined(__WXMAC__)
    #include  "wx/osx/private.h"  // includes mac headers
#endif

#ifdef __WINDOWS__
    #include "wx/msw/private.h"
    #include "wx/msw/missing.h"

    // sys/cygwin.h is needed for cygwin_conv_to_full_win32_path()
    // and for cygwin_conv_path()
    //
    // note that it must be included after <windows.h>
    #ifdef __GNUWIN32__
        #ifdef __CYGWIN__
            #include <sys/cygwin.h>
            #include <cygwin/version.h>
        #endif
    #endif // __GNUWIN32__

    // io.h is needed for _get_osfhandle()
    // Already included by filefn.h for many Windows compilers
    #if defined __CYGWIN__
        #include <io.h>
    #endif
#endif // __WINDOWS__

// TODO: Borland probably has _wgetcwd as well?
#ifdef _MSC_VER
    #define HAVE_WGETCWD
#endif

wxDECL_FOR_STRICT_MINGW32(int, _fileno, (FILE*))

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifndef _MAXPATHLEN
    #define _MAXPATHLEN 1024
#endif

// ============================================================================
// implementation
// ============================================================================

bool
wxFileExists (const wxString& filename)
{
    return wxFileName::FileExists(filename);
}

bool
wxIsAbsolutePath (const wxString& filename)
{
    if (!filename.empty())
    {
        // Unix like or Windows
        if (filename[0] == wxT('/'))
            return true;
#if defined(__WINDOWS__)
        // MSDOS like
        if (filename[0] == wxT('\\') || (wxIsalpha (filename[0]) && filename[1] == wxT(':')))
            return true;
#endif
    }
    return false ;
}

// Return just the filename, not the path (basename)
wxChar *wxFileNameFromPath (wxChar *path)
{
    wxString p = path;
    wxString n = wxFileNameFromPath(p);

    return path + p.length() - n.length();
}

wxString wxFileNameFromPath (const wxString& path)
{
    return wxFileName(path).GetFullName();
}

// Utility for converting delimiters in DOS filenames to UNIX style
// and back again - or we get nasty problems with delimiters.
// Also, convert to lower case, since case is significant in UNIX.

#if defined(__WXMAC__) && !defined(__WXOSX_IPHONE__)

#define kDefaultPathStyle kCFURLPOSIXPathStyle

wxString wxMacFSRefToPath( const FSRef *fsRef , CFStringRef additionalPathComponent )
{
    CFURLRef fullURLRef;
    fullURLRef = CFURLCreateFromFSRef(NULL, fsRef);
    if ( fullURLRef == NULL)
        return wxEmptyString;
    
    if ( additionalPathComponent )
    {
        CFURLRef parentURLRef = fullURLRef ;
        fullURLRef = CFURLCreateCopyAppendingPathComponent(NULL, parentURLRef,
            additionalPathComponent,false);
        CFRelease( parentURLRef ) ;
    }
    wxCFStringRef cfString( CFURLCopyFileSystemPath(fullURLRef, kDefaultPathStyle ));
    CFRelease( fullURLRef ) ;

    return wxCFStringRef::AsStringWithNormalizationFormC(cfString);
}

OSStatus wxMacPathToFSRef( const wxString&path , FSRef *fsRef )
{
    OSStatus err = noErr ;
    CFMutableStringRef cfMutableString = CFStringCreateMutableCopy(NULL, 0, wxCFStringRef(path));
    CFStringNormalize(cfMutableString,kCFStringNormalizationFormD);
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfMutableString , kDefaultPathStyle, false);
    CFRelease( cfMutableString );
    if ( NULL != url )
    {
        if ( CFURLGetFSRef(url, fsRef) == false )
            err = fnfErr ;
        CFRelease( url ) ;
    }
    else
    {
        err = fnfErr ;
    }
    return err ;
}

wxString wxMacHFSUniStrToString( ConstHFSUniStr255Param uniname )
{
    wxCFStringRef cfname( CFStringCreateWithCharacters( kCFAllocatorDefault,
                                                      uniname->unicode,
                                                      uniname->length ) );
    return wxCFStringRef::AsStringWithNormalizationFormC(cfname);
}

#ifndef __LP64__

wxString wxMacFSSpec2MacFilename( const FSSpec *spec )
{
    FSRef fsRef ;
    if ( FSpMakeFSRef( spec , &fsRef) == noErr )
    {
        return wxMacFSRefToPath( &fsRef ) ;
    }
    return wxEmptyString ;
}

void wxMacFilename2FSSpec( const wxString& path , FSSpec *spec )
{
    OSStatus err = noErr;
    FSRef fsRef;
    wxMacPathToFSRef( path , &fsRef );
    err = FSGetCatalogInfo(&fsRef, kFSCatInfoNone, NULL, NULL, spec, NULL);
    verify_noerr( err );
}
#endif

#endif // __WXMAC__

// helper of generic implementation of wxCopyFile()
#ifndef __WIN32__
static bool
wxDoCopyFile(wxFile& fileIn,
             const wxStructStat& fbuf,
             const wxString& filenameDst,
             bool overwrite)
{
    // reset the umask as we want to create the file with exactly the same
    // permissions as the original one
    wxCHANGE_UMASK(0);

    // create file2 with the same permissions than file1 and open it for
    // writing

    wxFile fileOut;
    if ( !fileOut.Create(filenameDst, overwrite, fbuf.st_mode & 0777) )
        return false;

    // copy contents of file1 to file2
    char buf[4096];
    for ( ;; )
    {
        ssize_t count = fileIn.Read(buf, WXSIZEOF(buf));
        if ( count == wxInvalidOffset )
            return false;

        // end of file?
        if ( !count )
            break;

        if ( fileOut.Write(buf, count) < (size_t)count )
            return false;
    }

    // we can expect fileIn to be closed successfully, but we should ensure
    // that fileOut was closed as some write errors (disk full) might not be
    // detected before doing this
    return fileIn.Close() && fileOut.Close();
}

#endif // generic implementation of wxCopyFile

// Copy files
bool
wxCopyFile (const wxString& file1, const wxString& file2, bool overwrite)
{
#if defined(__WIN32__)
    // CopyFile() copies file attributes and modification time too, so use it
    // instead of our code if available
    //
    // NB: 3rd parameter is bFailIfExists i.e. the inverse of overwrite
    if ( !::CopyFile(file1.t_str(), file2.t_str(), !overwrite) )
        return false;
#elif wxUSE_FILE // !Win32

    wxStructStat fbuf;
    // get permissions of file1
    if ( wxStat( file1, &fbuf) != 0 )
    {
        // the file probably doesn't exist or we haven't the rights to read
        // from it anyhow
        return false;
    }

    // open file1 for reading
    wxFile fileIn(file1, wxFile::read);
    if ( !fileIn.IsOpened() )
        return false;

    // remove file2, if it exists. This is needed for creating
    // file2 with the correct permissions in the next step
    if ( wxFileExists(file2)  && (!overwrite || !wxRemoveFile(file2)))
        return false;

    if ( !wxDoCopyFile(fileIn, fbuf, file2, overwrite) )
        return false;

#if defined(__WXMAC__)
    // copy the resource fork of the file too if it's present
    wxString pathRsrcOut;
    wxFile fileRsrcIn;

    {
        // it's not enough to check for file existence: it always does on HFS
        // but is empty for files without resources
        if ( fileRsrcIn.Open(file1 + wxT("/..namedfork/rsrc")) &&
                fileRsrcIn.Length() > 0 )
        {
            // we must be using HFS or another filesystem with resource fork
            // support, suppose that destination file system also is HFS[-like]
            pathRsrcOut = file2 + wxT("/..namedfork/rsrc");
        }
        else // check if we have resource fork in separate file (non-HFS case)
        {
            wxFileName fnRsrc(file1);
            fnRsrc.SetName(wxT("._") + fnRsrc.GetName());

            fileRsrcIn.Close();
            if ( fileRsrcIn.Open( fnRsrc.GetFullPath() ) )
            {
                fnRsrc = file2;
                fnRsrc.SetName(wxT("._") + fnRsrc.GetName());

                pathRsrcOut = fnRsrc.GetFullPath();
            }
        }
    }

    if ( !pathRsrcOut.empty() )
    {
        if ( !wxDoCopyFile(fileRsrcIn, fbuf, pathRsrcOut, overwrite) )
            return false;
    }
#endif // wxMac || wxCocoa

#if !defined(__WXMAC__) || defined(__UNIX__)
    // no chmod in VA.  Should be some permission API for HPFS386 partitions
    // however
    if ( chmod(file2.fn_str(), fbuf.st_mode) != 0 )
        return false;
#endif // OS/2 || Mac

#else // !Win32 && ! wxUSE_FILE

    // impossible to simulate with wxWidgets API
    wxUnusedVar(file1);
    wxUnusedVar(file2);
    wxUnusedVar(overwrite);
    return false;

#endif // __WINDOWS__ && __WIN32__

    return true;
}

bool
wxRenameFile(const wxString& file1, const wxString& file2, bool overwrite)
{
    if ( !overwrite && wxFileExists(file2) )
        return false;

    // Normal system call
  if ( wxRename (file1, file2) == 0 )
    return true;

  // Try to copy
  if (wxCopyFile(file1, file2, overwrite)) {
    wxRemoveFile(file1);
    return true;
  }
  // Give up
  return false;
}

bool wxRemoveFile(const wxString& file)
{
#if defined(__VISUALC__) \
 || defined(__GNUWIN32__)
    int res = wxRemove(file);
#elif defined(__WXMAC__)
    int res = unlink(file.fn_str());
#else
    int res = unlink(file.fn_str());
#endif
    return res == 0;
}

bool wxMkdir(const wxString& dir, int perm)
{
#if defined(__WXMAC__) && !defined(__UNIX__)
    if ( mkdir(dir.fn_str(), 0) != 0 )

    // assume mkdir() has 2 args on non Windows-OS/2 platforms and on Windows too
    // for the GNU compiler
#elif (!(defined(__WINDOWS__))) || \
      (defined(__GNUWIN32__) && !defined(__MINGW32__)) ||                \
      defined(__WINE__)
    const wxChar *dirname = dir.c_str();
  #if defined(MSVCRT)
    wxUnusedVar(perm);
    if ( mkdir(wxFNCONV(dirname)) != 0 )
  #else
    if ( mkdir(wxFNCONV(dirname), perm) != 0 )
  #endif
#else  // !MSW, !DOS and !OS/2 VAC++
    wxUnusedVar(perm);
    if ( wxMkDir(dir.fn_str()) != 0 )
#endif // !MSW/MSW
    {
        return false;
    }

    return true;
}

bool wxRmdir(const wxString& dir, int WXUNUSED(flags))
{
    if ( wxRmDir(dir.fn_str()) != 0 )
        return false;

    return true;
}

// does the path exists? (may have or not '/' or '\\' at the end)
bool wxDirExists(const wxString& pathName)
{
    return wxFileName::DirExists(pathName);
}

// Get current working directory.
// If buf is NULL, allocates space using new, else copies into buf.
// wxGetWorkingDirectory() is obsolete, use wxGetCwd()
// wxDoGetCwd() is their common core to be moved
// to wxGetCwd() once wxGetWorkingDirectory() will be removed.
// Do not expose wxDoGetCwd in headers!

wxChar *wxDoGetCwd(wxChar *buf, int sz)
{
    if ( !buf )
        buf = new wxChar[sz + 1];

    bool ok = false;

    // for the compilers which have Unicode version of _getcwd(), call it
    // directly, for the others call the ANSI version and do the translation
#if !wxUSE_UNICODE
    #define cbuf buf
#else // wxUSE_UNICODE
    bool needsANSI = true;

    #if !defined(HAVE_WGETCWD)
        char cbuf[_MAXPATHLEN];
    #endif

    #ifdef HAVE_WGETCWD
            char *cbuf = NULL; // never really used because needsANSI will always be false
            {
                ok = _wgetcwd(buf, sz) != NULL;
                needsANSI = false;
            }
    #endif

    if ( needsANSI )
#endif // wxUSE_UNICODE
    {
    #if defined(_MSC_VER) || defined(__MINGW32__)
        ok = _getcwd(cbuf, sz) != NULL;
    #else // !Win32/VC++ !Mac
        ok = getcwd(cbuf, sz) != NULL;
    #endif // platform

    #if wxUSE_UNICODE
        // finally convert the result to Unicode if needed
        wxConvFile.MB2WC(buf, cbuf, sz);
    #endif // wxUSE_UNICODE
    }

    if ( !ok )
    {
        // VZ: the old code used to return "." on error which didn't make any
        //     sense at all to me - empty string is a better error indicator
        //     (NULL might be even better but I'm afraid this could lead to
        //     problems with the old code assuming the return is never NULL)
        buf[0] = wxT('\0');
    }
    else // ok, but we might need to massage the path into the right format
    {
// MBN: we hope that in the case the user is compiling a GTK+/Motif app,
//      he needs Unix as opposed to Win32 pathnames
#if defined( __CYGWIN__ ) && defined( __WINDOWS__ )
        // another example of DOS/Unix mix (Cygwin)
        wxString pathUnix = buf;
#if wxUSE_UNICODE
    #if CYGWIN_VERSION_DLL_MAJOR >= 1007
        cygwin_conv_path(CCP_POSIX_TO_WIN_W, pathUnix.mb_str(wxConvFile), buf, sz);
    #else
        char bufA[_MAXPATHLEN];
        cygwin_conv_to_full_win32_path(pathUnix.mb_str(wxConvFile), bufA);
        wxConvFile.MB2WC(buf, bufA, sz);
    #endif
#else
    #if CYGWIN_VERSION_DLL_MAJOR >= 1007
        cygwin_conv_path(CCP_POSIX_TO_WIN_A, pathUnix, buf, sz);
    #else
        cygwin_conv_to_full_win32_path(pathUnix, buf);
    #endif
#endif // wxUSE_UNICODE
#endif // __CYGWIN__
    }

    return buf;

#if !wxUSE_UNICODE
    #undef cbuf
#endif
}

wxString wxGetCwd()
{
    wxString str;
    wxDoGetCwd(wxStringBuffer(str, _MAXPATHLEN), _MAXPATHLEN);
    return str;
}

bool wxSetWorkingDirectory(const wxString& d)
{
    bool success = false;
#if defined(__UNIX__) || defined(__WXMAC__)
    success = (chdir(wxFNSTRINGCAST d.fn_str()) == 0);
#elif defined(__WINDOWS__)

#ifdef __WIN32__
    success = (SetCurrentDirectory(d.t_str()) != 0);
#else
    // Must change drive, too.
    bool isDriveSpec = ((strlen(d) > 1) && (d[1] == ':'));
    if (isDriveSpec)
    {
        wxChar firstChar = d[0];

        // To upper case
        if (firstChar > 90)
            firstChar = firstChar - 32;

        // To a drive number
        unsigned int driveNo = firstChar - 64;
        if (driveNo > 0)
        {
            unsigned int noDrives;
            _dos_setdrive(driveNo, &noDrives);
        }
    }
    success = (chdir(WXSTRINGCAST d) == 0);
#endif

#endif
    return success;
}

bool wxEndsWithPathSeparator(const wxString& filename)
{
    return !filename.empty() && wxIsPathSeparator(filename.Last());
}

#if defined(__WINDOWS__) && !(defined(__UNIX__))
static bool wxCheckWin32Permission(const wxString& path, DWORD access)
{
    // quoting the MSDN: "To obtain a handle to a directory, call the
    // CreateFile function with the FILE_FLAG_BACKUP_SEMANTICS flag", but this
    // doesn't work under Win9x/ME but then it's not needed there anyhow
    const DWORD dwAttr = ::GetFileAttributes(path.t_str());
    if ( dwAttr == INVALID_FILE_ATTRIBUTES )
    {
        // file probably doesn't exist at all
        return false;
    }

    HANDLE h = ::CreateFile
                 (
                    path.t_str(),
                    access,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    dwAttr & FILE_ATTRIBUTE_DIRECTORY
                        ? FILE_FLAG_BACKUP_SEMANTICS
                        : 0,
                    NULL
                 );
    if ( h != INVALID_HANDLE_VALUE )
        CloseHandle(h);

    return h != INVALID_HANDLE_VALUE;
}
#endif // __WINDOWS__

bool wxIsWritable(const wxString &path)
{
#if defined( __UNIX__ )
    // access() will take in count also symbolic links
    return wxAccess(path.c_str(), W_OK) == 0;
#elif defined( __WINDOWS__ )
    return wxCheckWin32Permission(path, GENERIC_WRITE);
#else
    wxUnusedVar(path);
    // TODO
    return false;
#endif
}

bool wxIsReadable(const wxString &path)
{
#if defined( __UNIX__ )
    // access() will take in count also symbolic links
    return wxAccess(path.c_str(), R_OK) == 0;
#elif defined( __WINDOWS__ )
    return wxCheckWin32Permission(path, GENERIC_READ);
#else
    wxUnusedVar(path);
    // TODO
    return false;
#endif
}

// Return the type of an open file
//
// Some file types on some platforms seem seekable but in fact are not.
// The main use of this function is to allow such cases to be detected
// (IsSeekable() is implemented as wxGetFileKind() == wxFILE_KIND_DISK).
//
// This is important for the archive streams, which benefit greatly from
// being able to seek on a stream, but which will produce corrupt archives
// if they unknowingly seek on a non-seekable stream.
//
// wxFILE_KIND_DISK is a good catch all return value, since other values
// disable features of the archive streams. Some other value must be returned
// for a file type that appears seekable but isn't.
//
// Known examples:
//   *  Pipes on Windows
//
wxFileKind wxGetFileKind(int fd)
{
#if defined __WINDOWS__ && defined wxGetOSFHandle
    switch (::GetFileType(wxGetOSFHandle(fd)) & ~FILE_TYPE_REMOTE)
    {
        case FILE_TYPE_CHAR:
            return wxFILE_KIND_TERMINAL;
        case FILE_TYPE_DISK:
            return wxFILE_KIND_DISK;
        case FILE_TYPE_PIPE:
            return wxFILE_KIND_PIPE;
    }

    return wxFILE_KIND_UNKNOWN;

#elif defined(__UNIX__)
    if (isatty(fd))
        return wxFILE_KIND_TERMINAL;

    struct stat st;
    fstat(fd, &st);

    if (S_ISFIFO(st.st_mode))
        return wxFILE_KIND_PIPE;
    if (!S_ISREG(st.st_mode))
        return wxFILE_KIND_UNKNOWN;

    return wxFILE_KIND_DISK;

#else
    #define wxFILEKIND_STUB
    (void)fd;
    return wxFILE_KIND_DISK;
#endif
}

wxFileKind wxGetFileKind(FILE *fp)
{
    // Note: The watcom rtl dll doesn't have fileno (the static lib does).
    //       Should be fixed in version 1.4.
#if defined(wxFILEKIND_STUB)
    (void)fp;
    return wxFILE_KIND_DISK;
#elif defined(__WINDOWS__) && !defined(__CYGWIN__)
    return fp ? wxGetFileKind(_fileno(fp)) : wxFILE_KIND_UNKNOWN;
#else
    return fp ? wxGetFileKind(fileno(fp)) : wxFILE_KIND_UNKNOWN;
#endif
}


//------------------------------------------------------------------------
// wild character routines
//------------------------------------------------------------------------

/*
* Written By Douglas A. Lewis <dalewis@cs.Buffalo.EDU>
*
* The match procedure is public domain code (from ircII's reg.c)
* but modified to suit our tastes (RN: No "%" syntax I guess)
*/

bool wxMatchWild( const wxString& pat, const wxString& text, bool dot_special )
{
    if (text.empty())
    {
        /* Match if both are empty. */
        return pat.empty();
    }

    const wxChar *m = pat.c_str(),
    *n = text.c_str(),
    *ma = NULL,
    *na = NULL;
    int just = 0,
    acount = 0,
    count = 0;

    if (dot_special && (*n == wxT('.')))
    {
        /* Never match so that hidden Unix files
         * are never found. */
        return false;
    }

    for (;;)
    {
        if (*m == wxT('*'))
        {
            ma = ++m;
            na = n;
            just = 1;
            acount = count;
        }
        else if (*m == wxT('?'))
        {
            m++;
            if (!*n++)
                return false;
        }
        else
        {
            if (*m == wxT('\\'))
            {
                m++;
                /* Quoting "nothing" is a bad thing */
                if (!*m)
                    return false;
            }
            if (!*m)
            {
                /*
                * If we are out of both strings or we just
                * saw a wildcard, then we can say we have a
                * match
                */
                if (!*n)
                    return true;
                if (just)
                    return true;
                just = 0;
                goto not_matched;
            }
            /*
            * We could check for *n == NULL at this point, but
            * since it's more common to have a character there,
            * check to see if they match first (m and n) and
            * then if they don't match, THEN we can check for
            * the NULL of n
            */
            just = 0;
            if (*m == *n)
            {
                m++;
                count++;
                n++;
            }
            else
            {

                not_matched:

                /*
                * If there are no more characters in the
                * string, but we still need to find another
                * character (*m != NULL), then it will be
                * impossible to match it
                */
                if (!*n)
                    return false;

                if (ma)
                {
                    m = ma;
                    n = ++na;
                    count = acount;
                }
                else
                    return false;
            }
        }
    }
}

#ifdef __VISUALC__
    #pragma warning(default:4706)   // assignment within conditional expression
#endif // VC++
