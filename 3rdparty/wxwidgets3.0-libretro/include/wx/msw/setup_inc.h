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

// Set this to 1 to enable wxActiveXContainer class allowing to embed OLE
// controls in wx.
//
// Default is 1.
//
// Recommended setting: 1, required by wxMediaCtrl
#define wxUSE_ACTIVEX 1

// wxDC caching implementation
#define wxUSE_DC_CACHEING 1

// Set this to 1 to enable wxDIB class used internally for manipulating
// wxBitmap data.
//
// Default is 1, set it to 0 only if you don't use wxImage neither
//
// Recommended setting: 1 (without it conversion to/from wxImage won't work)
#define wxUSE_WXDIB 1

// Set this to 1 to compile in wxRegKey class.
//
// Default is 1
//
// Recommended setting: 1, this is used internally by wx in a few places
#define wxUSE_REGKEY 1

// ----------------------------------------------------------------------------
// Generic versions of native controls
// ----------------------------------------------------------------------------

// Set this to 1 to be able to use wxDatePickerCtrlGeneric in addition to the
// native wxDatePickerCtrl
//
// Default is 0.
//
// Recommended setting: 0, this is mainly used for testing
#define wxUSE_DATEPICKCTRL_GENERIC 0

// Set this to 1 to be able to use wxTimePickerCtrlGeneric in addition to the
// native wxTimePickerCtrl for the platforms that have the latter (MSW).
//
// Default is 0.
//
// Recommended setting: 0, this is mainly used for testing
#define wxUSE_TIMEPICKCTRL_GENERIC 0
