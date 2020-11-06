///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/cmdline.cpp
// Purpose:     wxCmdLineParser implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05.01.00
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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
    #include "wx/dynarray.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/app.h"
#endif //WX_PRECOMP

#include "wx/cmdline.h"

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

/*
   This function is mainly used under Windows (as under Unix we always get the
   command line arguments as argc/argv anyhow) and so it tries to follow
   Windows conventions for the command line handling, not Unix ones. For
   instance, backslash is not special except when it precedes double quote when
   it does quote it.

   TODO: Rewrite this to follow the even more complicated rule used by Windows
         CommandLineToArgv():

    * A string of backslashes not followed by a quotation mark has no special
      meaning.
    * An even number of backslashes followed by a quotation mark is treated as
      pairs of protected backslashes, followed by a word terminator.
    * An odd number of backslashes followed by a quotation mark is treated as
      pairs of protected backslashes, followed by a protected quotation mark.

    See http://blogs.msdn.com/b/oldnewthing/archive/2010/09/17/10063629.aspx

    It could also be useful to provide a converse function which is also
    non-trivial, see

    http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/04/23/everyone-quotes-arguments-the-wrong-way.aspx
 */

/* static */
wxArrayString
wxCmdLineParser::ConvertStringToArgs(const wxString& cmdline,
                                     wxCmdLineSplitType type)
{
    wxArrayString args;

    wxString arg;
    arg.reserve(1024);

    const wxString::const_iterator end = cmdline.end();
    wxString::const_iterator p = cmdline.begin();

    for ( ;; )
    {
        // skip white space
        while ( p != end && (*p == ' ' || *p == '\t') )
            ++p;

        // anything left?
        if ( p == end )
            break;

        // parse this parameter
        bool lastBS = false,
             isInsideQuotes = false;
        wxChar chDelim = '\0';
        for ( arg.clear(); p != end; ++p )
        {
            const wxChar ch = *p;

            if ( type == wxCMD_LINE_SPLIT_DOS )
            {
                if ( ch == '"' )
                {
                    if ( !lastBS )
                    {
                        isInsideQuotes = !isInsideQuotes;

                        // don't put quote in arg
                        continue;
                    }
                    //else: quote has no special meaning but the backslash
                    //      still remains -- makes no sense but this is what
                    //      Windows does
                }
                // note that backslash does *not* quote the space, only quotes do
                else if ( !isInsideQuotes && (ch == ' ' || ch == '\t') )
                {
                    ++p;    // skip this space anyhow
                    break;
                }

                lastBS = !lastBS && ch == '\\';
            }
            else // type == wxCMD_LINE_SPLIT_UNIX
            {
                if ( !lastBS )
                {
                    if ( isInsideQuotes )
                    {
                        if ( ch == chDelim )
                        {
                            isInsideQuotes = false;

                            continue;   // don't use the quote itself
                        }
                    }
                    else // not in quotes and not escaped
                    {
                        if ( ch == '\'' || ch == '"' )
                        {
                            isInsideQuotes = true;
                            chDelim = ch;

                            continue;   // don't use the quote itself
                        }

                        if ( ch == ' ' || ch == '\t' )
                        {
                            ++p;    // skip this space anyhow
                            break;
                        }
                    }

                    lastBS = ch == '\\';
                    if ( lastBS )
                        continue;
                }
                else // escaped by backslash, just use as is
                {
                    lastBS = false;
                }
            }

            arg += ch;
        }

        args.push_back(arg);
    }

    return args;
}
