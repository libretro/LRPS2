///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/setup_inc.h
// Purpose:     MSW-specific setup.h options
// Author:      Vadim Zeitlin
// Created:     2007-07-21 (extracted from wx/msw/setup0.h)
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// Windows-only settings
// ----------------------------------------------------------------------------

// Set wxUSE_UNICODE_MSLU to 1 if you're compiling wxWidgets in Unicode mode
// and want to run your programs under Windows 9x and not only NT/2000/XP.
// This setting enables use of unicows.dll from MSLU (MS Layer for Unicode, see
// http://www.microsoft.com/globaldev/handson/dev/mslu_announce.mspx). Note
// that you will have to modify the makefiles to include unicows.lib import
// library as the first library (see installation instructions in install.txt
// to learn how to do it when building the library or samples).
//
// If your compiler doesn't have unicows.lib, you can get a version of it at
// http://libunicows.sourceforge.net
//
// Default is 0
//
// Recommended setting: 0 (1 if you want to deploy Unicode apps on 9x systems)
#ifndef wxUSE_UNICODE_MSLU
    #define wxUSE_UNICODE_MSLU 0
#endif

// Set this to 1 if you want to use wxWidgets and MFC in the same program. This
// will override some other settings (see below)
//
// Default is 0.
//
// Recommended setting: 0 unless you really have to use MFC
#define wxUSE_MFC           0
