///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cmdline.h
// Purpose:     wxCmdLineParser and related classes for parsing the command
//              line options
// Author:      Vadim Zeitlin
// Modified by:
// Created:     04.01.00
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CMDLINE_H_
#define _WX_CMDLINE_H_

#include "wx/defs.h"

#include "wx/string.h"
#include "wx/arrstr.h"
#include "wx/cmdargs.h"

// determines ConvertStringToArgs() behaviour
enum wxCmdLineSplitType
{
    wxCMD_LINE_SPLIT_DOS,
    wxCMD_LINE_SPLIT_UNIX
};

// this function is always available (even if !wxUSE_CMDLINE_PARSER) because it
// is used by wxWin itself under Windows
class WXDLLIMPEXP_BASE wxCmdLineParser
{
public:
    static wxArrayString
    ConvertStringToArgs(const wxString& cmdline,
                        wxCmdLineSplitType type = wxCMD_LINE_SPLIT_DOS);
};

#endif // _WX_CMDLINE_H_
