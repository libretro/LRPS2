/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/dir.cpp
// Purpose:     wxDir implementation for Unix/POSIX systems
// Author:      Vadim Zeitlin
// Modified by:
// Created:     08.12.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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

#include "wx/dir.h"
#include "wx/filefn.h"          // for wxMatchWild
#include "wx/filename.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dirent.h>

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

#define M_DIR       ((wxDirData *)m_data)

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// this class stores everything we need to enumerate the files
class wxDirData
{
public:
    wxDirData(const wxString& dirname);
    ~wxDirData();

    bool IsOk() const { return m_dir != NULL; }

    void SetFileSpec(const wxString& filespec) { m_filespec = filespec; }
    void SetFlags(int flags) { m_flags = flags; }

    void Rewind() { rewinddir(m_dir); }
    bool Read(wxString *filename);

    const wxString& GetName() const { return m_dirname; }

private:
    DIR     *m_dir;

    wxString m_dirname;
    wxString m_filespec;

    int      m_flags;
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDirData
// ----------------------------------------------------------------------------

wxDirData::wxDirData(const wxString& dirname)
         : m_dirname(dirname)
{
    m_dir = NULL;

    // throw away the trailing slashes
    size_t n = m_dirname.length();
    while ( n > 0 && m_dirname[--n] == '/' );

    m_dirname.Truncate(n + 1);

    // do open the dir
    m_dir = opendir(m_dirname.fn_str());
}

wxDirData::~wxDirData()
{
    if ( m_dir )
        if ( closedir(m_dir) != 0 ) { }
}

bool wxDirData::Read(wxString *filename)
{
    dirent *de = NULL;    // just to silence compiler warnings
    bool matches = false;

    // speed up string concatenation in the loop a bit
    wxString path = m_dirname;
    path += wxT('/');
    path.reserve(path.length() + 255);

    wxString de_d_name;

    while ( !matches )
    {
        de = readdir(m_dir);
        if ( !de )
            return false;

#if wxUSE_UNICODE
        de_d_name = wxString(de->d_name, *wxConvFileName);
#else
        de_d_name = de->d_name;
#endif

        // don't return "." and ".." unless asked for
        if ( de->d_name[0] == '.' &&
             ((de->d_name[1] == '.' && de->d_name[2] == '\0') ||
              (de->d_name[1] == '\0')) )
        {
            if ( !(m_flags & wxDIR_DOTDOT) )
                continue;

            // we found a valid match
            break;
        }

        // check the type now: notice that we may want to check the type of
        // the path itself and not whatever it points to in case of a symlink
        wxFileName fn = wxFileName::DirName(path + de_d_name);
        if ( m_flags & wxDIR_NO_FOLLOW )
        {
            fn.DontFollowLink();
        }

        if ( !(m_flags & wxDIR_FILES) && !fn.DirExists() )
        {
            // it's a file, but we don't want them
            continue;
        }
        else if ( !(m_flags & wxDIR_DIRS) && fn.DirExists() )
        {
            // it's a dir, and we don't want it
            continue;
        }

        // finally, check the name
        if ( m_filespec.empty() )
        {
            matches = m_flags & wxDIR_HIDDEN ? true : de->d_name[0] != '.';
        }
        else
        {
            // test against the pattern
            matches = wxMatchWild(m_filespec, de_d_name,
                                  !(m_flags & wxDIR_HIDDEN));
        }
    }

    *filename = de_d_name;

    return true;
}

// ----------------------------------------------------------------------------
// wxDir construction/destruction
// ----------------------------------------------------------------------------

wxDir::wxDir(const wxString& dirname)
{
    m_data = NULL;

    (void)Open(dirname);
}

bool wxDir::Open(const wxString& dirname)
{
    delete M_DIR;
    m_data = new wxDirData(dirname);

    if ( !M_DIR->IsOk() )
    {
        delete M_DIR;
        m_data = NULL;

        return false;
    }

    return true;
}

bool wxDir::IsOpened() const
{
    return m_data != NULL;
}

wxString wxDir::GetName() const
{
    wxString name;
    if ( m_data )
    {
        name = M_DIR->GetName();

        // Notice that we need to check for length > 1 as we shouldn't remove
        // the last slash from the root directory!
        if ( name.length() > 1 && (name.Last() == wxT('/')) )
        {
            // chop off the last slash
            name.RemoveLast();
        }
    }

    return name;
}

void wxDir::Close()
{
    if ( m_data )
    {
        delete m_data;
        m_data = NULL;
    }
}

// ----------------------------------------------------------------------------
// wxDir enumerating
// ----------------------------------------------------------------------------

bool wxDir::GetFirst(wxString *filename,
                     const wxString& filespec,
                     int flags) const
{
    M_DIR->Rewind();

    M_DIR->SetFileSpec(filespec);
    M_DIR->SetFlags(flags);

    return GetNext(filename);
}

bool wxDir::GetNext(wxString *filename) const
{
    return M_DIR->Read(filename);
}
