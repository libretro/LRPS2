///////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/stdpaths.cpp
// Purpose:     wxStandardPaths implementation for Unix & OpenVMS systems
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-10-19
// Copyright:   (c) 2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STDPATHS

#include "wx/stdpaths.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/wxcrt.h"
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/filename.h"
#include "wx/textfile.h"

#if defined( __LINUX__ ) || defined( __VMS )
    #include <unistd.h>
#endif

// ============================================================================
// common VMS/Unix part of wxStandardPaths implementation
// ============================================================================

wxString wxStandardPaths::GetUserConfigDir() const
{
    return wxFileName::GetHomeDir();
}


// ============================================================================
// wxStandardPaths implementation for VMS
// ============================================================================

#ifdef __VMS
wxString wxStandardPaths::GetConfigDir() const
{
   return wxT("/sys$manager");
}

wxString wxStandardPaths::GetUserDataDir() const
{
   return wxFileName::GetHomeDir();
}
#else // !__VMS
// ----------------------------------------------------------------------------
// public functions
// ----------------------------------------------------------------------------

wxString wxStandardPaths::GetConfigDir() const
{
   return wxT("/etc");
}

wxString wxStandardPaths::GetUserDataDir() const
{
   return AppendAppInfo(wxFileName::GetHomeDir() + wxT("/."));
}

wxString wxStandardPaths::GetDocumentsDir() const
{
    {
        wxString homeDir = wxFileName::GetHomeDir();
        wxString configPath;
        if (wxGetenv(wxT("XDG_CONFIG_HOME")))
            configPath = wxGetenv(wxT("XDG_CONFIG_HOME"));
        else
            configPath = homeDir + wxT("/.config");
        wxString dirsFile = configPath + wxT("/user-dirs.dirs");
        if (wxFileExists(dirsFile))
        {
            wxTextFile textFile;
            if (textFile.Open(dirsFile))
            {
                size_t i;
                for (i = 0; i < textFile.GetLineCount(); i++)
                {
                    wxString line(textFile[i]);
                    int pos = line.Find(wxT("XDG_DOCUMENTS_DIR"));
                    if (pos != wxNOT_FOUND)
                    {
                        wxString value = line.AfterFirst(wxT('='));
                        value.Replace(wxT("$HOME"), homeDir);
                        value.Trim(true);
                        value.Trim(false);
                        if (!value.IsEmpty() && wxDirExists(value))
                            return value;
                        else
                            break;
                    }
                }
            }
        }
    }

    return wxStandardPathsBase::GetDocumentsDir();
}
#endif // __VMS/!__VMS

#endif // wxUSE_STDPATHS
