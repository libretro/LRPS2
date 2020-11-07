/*
 * Name:        wx/chkconf.h
 * Purpose:     check the config settings for consistency
 * Author:      Vadim Zeitlin
 * Modified by:
 * Created:     09.08.00
 * Copyright:   (c) 2000 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */
#ifndef _WX_CHKCONF_H_
#define _WX_CHKCONF_H_

/*
              **************************************************
              PLEASE READ THIS IF YOU GET AN ERROR IN THIS FILE!
              **************************************************

    If you get an error saying "wxUSE_FOO must be defined", it means that you
    are not using the correct up-to-date version of setup.h. This happens most
    often when using svn or daily snapshots and a new symbol was added to
    setup0.h and you haven't updated your local setup.h to reflect it. If
    this is the case, you need to propagate the changes from setup0.h to your
    setup.h and, if using makefiles under MSW, also remove setup.h under the
    build directory (lib/$(COMPILER)_{lib,dll}/msw[u][d][dll]/wx) so that
    the new setup.h is copied there.

    If you get an error of the form "wxFoo requires wxBar", then the settings
    in your setup.h are inconsistent. You have the choice between correcting
    them manually or commenting out #define wxABORT_ON_CONFIG_ERROR below to
    try to correct the problems automatically (not really recommended but
    might work).
 */

/*
   This file has the following sections:
    1. checks that all wxUSE_XXX symbols we use are defined
     a) first the non-GUI ones
     b) then the GUI-only ones
    2. platform-specific checks done in the platform headers
    3. generic consistency checks
     a) first the non-GUI ones
     b) then the GUI-only ones
 */

/*
   global features
 */

/*
    If we're compiling without support for threads/exceptions we have to
    disable the corresponding features.
 */
#ifdef wxNO_THREADS
#   undef wxUSE_THREADS
#   define wxUSE_THREADS 0
#endif /* wxNO_THREADS */

/*
   Section 1a: tests for non GUI features.

   please keep the options in alphabetical order!
 */

#ifndef wxUSE_ANY
#define wxUSE_ANY 0
#endif /* wxUSE_ANY */

#ifndef wxUSE_COMPILER_TLS
#define wxUSE_COMPILER_TLS 0
#endif /* !defined(wxUSE_COMPILER_TLS) */

#ifndef wxUSE_CONSOLE_EVENTLOOP
#define wxUSE_CONSOLE_EVENTLOOP 0
#endif /* !defined(wxUSE_CONSOLE_EVENTLOOP) */

#ifndef wxUSE_DYNLIB_CLASS
#define wxUSE_DYNLIB_CLASS 0
#endif /* !defined(wxUSE_DYNLIB_CLASS) */

#ifndef wxUSE_FILE_HISTORY
#define wxUSE_FILE_HISTORY 0
#endif /* !defined(wxUSE_FILE_HISTORY) */

#ifndef wxUSE_FILESYSTEM
#define wxUSE_FILESYSTEM 0
#endif /* !defined(wxUSE_FILESYSTEM) */

#ifndef wxUSE_FS_ARCHIVE
#define wxUSE_FS_ARCHIVE 0
#endif /* !defined(wxUSE_FS_ARCHIVE) */

#ifndef wxUSE_FSVOLUME
#define wxUSE_FSVOLUME 0
#endif /* !defined(wxUSE_FSVOLUME) */

#ifndef wxUSE_FSWATCHER
#define wxUSE_FSWATCHER 0
#endif /* !defined(wxUSE_FSWATCHER) */

#ifndef wxUSE_DYNAMIC_LOADER
#       define wxUSE_DYNAMIC_LOADER 0
#endif /* !defined(wxUSE_DYNAMIC_LOADER) */

#ifndef wxUSE_INTL
#       define wxUSE_INTL 0
#endif /* !defined(wxUSE_INTL) */

#ifndef wxUSE_IPV6
#       define wxUSE_IPV6 0
#endif /* !defined(wxUSE_IPV6) */

#ifndef wxUSE_LONGLONG
#       define wxUSE_LONGLONG 0
#endif /* !defined(wxUSE_LONGLONG) */

#ifndef wxUSE_ON_FATAL_EXCEPTION
#       define wxUSE_ON_FATAL_EXCEPTION 0
#endif /* !defined(wxUSE_ON_FATAL_EXCEPTION) */

#ifndef wxUSE_PRINTF_POS_PARAMS
#       define wxUSE_PRINTF_POS_PARAMS 0
#endif /* !defined(wxUSE_PRINTF_POS_PARAMS) */

#ifndef wxUSE_PROTOCOL
#       define wxUSE_PROTOCOL 0
#endif /* !defined(wxUSE_PROTOCOL) */

/* we may not define wxUSE_PROTOCOL_XXX if wxUSE_PROTOCOL is set to 0 */
#if !wxUSE_PROTOCOL
#   undef wxUSE_PROTOCOL_HTTP
#   undef wxUSE_PROTOCOL_FTP
#   undef wxUSE_PROTOCOL_FILE
#   define wxUSE_PROTOCOL_HTTP 0
#   define wxUSE_PROTOCOL_FTP 0
#   define wxUSE_PROTOCOL_FILE 0
#endif /* wxUSE_PROTOCOL */

#ifndef wxUSE_PROTOCOL_HTTP
#       define wxUSE_PROTOCOL_HTTP 0
#endif /* !defined(wxUSE_PROTOCOL_HTTP) */

#ifndef wxUSE_PROTOCOL_FTP
#       define wxUSE_PROTOCOL_FTP 0
#endif /* !defined(wxUSE_PROTOCOL_FTP) */

#ifndef wxUSE_PROTOCOL_FILE
#       define wxUSE_PROTOCOL_FILE 0
#endif /* !defined(wxUSE_PROTOCOL_FILE) */

#ifndef wxUSE_REGEX
#       define wxUSE_REGEX 0
#endif /* !defined(wxUSE_REGEX) */

#ifndef wxUSE_STDPATHS
#       define wxUSE_STDPATHS 1
#endif /* !defined(wxUSE_STDPATHS) */

#ifndef wxUSE_XML
#       define wxUSE_XML 0
#endif /* !defined(wxUSE_XML) */

#ifndef wxUSE_STD_CONTAINERS
#       define wxUSE_STD_CONTAINERS 0
#endif /* !defined(wxUSE_STD_CONTAINERS) */

#ifndef wxUSE_STD_STRING_CONV_IN_WXSTRING
#       define wxUSE_STD_STRING_CONV_IN_WXSTRING 0
#endif /* !defined(wxUSE_STD_STRING_CONV_IN_WXSTRING) */

#ifndef wxUSE_STREAMS
#       define wxUSE_STREAMS 0
#endif /* !defined(wxUSE_STREAMS) */

#ifndef wxUSE_STOPWATCH
#       define wxUSE_STOPWATCH 0
#endif /* !defined(wxUSE_STOPWATCH) */

#ifndef wxUSE_TEXTBUFFER
#       define wxUSE_TEXTBUFFER 0
#endif /* !defined(wxUSE_TEXTBUFFER) */

#ifndef wxUSE_TEXTFILE
#       define wxUSE_TEXTFILE 0
#endif /* !defined(wxUSE_TEXTFILE) */

#ifndef wxUSE_UNICODE
#       define wxUSE_UNICODE 0
#endif /* !defined(wxUSE_UNICODE) */

#ifndef wxUSE_URL
#       define wxUSE_URL 0
#endif /* !defined(wxUSE_URL) */

#ifndef wxUSE_VARIANT
#       define wxUSE_VARIANT 0
#endif /* wxUSE_VARIANT */

#ifndef wxUSE_XLOCALE
#       define wxUSE_XLOCALE 0
#endif /* !defined(wxUSE_XLOCALE) */

/*
   Section 1b: all these tests are for GUI only.

   please keep the options in alphabetical order!
 */
#if wxUSE_GUI

/*
   all of the settings tested below must be defined or we'd get an error from
   preprocessor about invalid integer expression
 */

#ifndef wxUSE_ABOUTDLG
#       define wxUSE_ABOUTDLG 0
#endif /* !defined(wxUSE_ABOUTDLG) */

#ifndef wxUSE_ACCEL
#       define wxUSE_ACCEL 0
#endif /* !defined(wxUSE_ACCEL) */

#ifndef wxUSE_ACCESSIBILITY
#       define wxUSE_ACCESSIBILITY 0
#endif /* !defined(wxUSE_ACCESSIBILITY) */

#ifndef wxUSE_ANIMATIONCTRL
#       define wxUSE_ANIMATIONCTRL 0
#endif /* !defined(wxUSE_ANIMATIONCTRL) */

#ifndef wxUSE_ARTPROVIDER_STD
#       define wxUSE_ARTPROVIDER_STD 0
#endif /* !defined(wxUSE_ARTPROVIDER_STD) */

#ifndef wxUSE_ARTPROVIDER_TANGO
#       define wxUSE_ARTPROVIDER_TANGO 0
#endif /* !defined(wxUSE_ARTPROVIDER_TANGO) */

#ifndef wxUSE_AUTOID_MANAGEMENT
#       define wxUSE_AUTOID_MANAGEMENT 0
#endif /* !defined(wxUSE_AUTOID_MANAGEMENT) */

#ifndef wxUSE_BITMAPCOMBOBOX
#       define wxUSE_BITMAPCOMBOBOX 0
#endif /* !defined(wxUSE_BITMAPCOMBOBOX) */

#ifndef wxUSE_BMPBUTTON
#       define wxUSE_BMPBUTTON 0
#endif /* !defined(wxUSE_BMPBUTTON) */

#ifndef wxUSE_BUTTON
#       define wxUSE_BUTTON 0
#endif /* !defined(wxUSE_BUTTON) */

#ifndef wxUSE_CAIRO
#       define wxUSE_CAIRO 0
#endif /* !defined(wxUSE_CAIRO) */

#ifndef wxUSE_CALENDARCTRL
#       define wxUSE_CALENDARCTRL 0
#endif /* !defined(wxUSE_CALENDARCTRL) */

#ifndef wxUSE_CARET
#       define wxUSE_CARET 0
#endif /* !defined(wxUSE_CARET) */

#ifndef wxUSE_CHECKBOX
#       define wxUSE_CHECKBOX 0
#endif /* !defined(wxUSE_CHECKBOX) */

#ifndef wxUSE_CHECKLISTBOX
#       define wxUSE_CHECKLISTBOX 0
#endif /* !defined(wxUSE_CHECKLISTBOX) */

#ifndef wxUSE_CHOICE
#       define wxUSE_CHOICE 0
#endif /* !defined(wxUSE_CHOICE) */

#ifndef wxUSE_CHOICEBOOK
#       define wxUSE_CHOICEBOOK 0
#endif /* !defined(wxUSE_CHOICEBOOK) */

#ifndef wxUSE_CHOICEDLG
#       define wxUSE_CHOICEDLG 0
#endif /* !defined(wxUSE_CHOICEDLG) */

#ifndef wxUSE_CLIPBOARD
#       define wxUSE_CLIPBOARD 0
#endif /* !defined(wxUSE_CLIPBOARD) */

#ifndef wxUSE_COLLPANE
#       define wxUSE_COLLPANE 0
#endif /* !defined(wxUSE_COLLPANE) */

#ifndef wxUSE_COLOURDLG
#       define wxUSE_COLOURDLG 0
#endif /* !defined(wxUSE_COLOURDLG) */

#ifndef wxUSE_COLOURPICKERCTRL
#       define wxUSE_COLOURPICKERCTRL 0
#endif /* !defined(wxUSE_COLOURPICKERCTRL) */

#ifndef wxUSE_COMBOBOX
#       define wxUSE_COMBOBOX 0
#endif /* !defined(wxUSE_COMBOBOX) */

#ifndef wxUSE_COMMANDLINKBUTTON
#       define wxUSE_COMMANDLINKBUTTON 0
#endif /* !defined(wxUSE_COMMANDLINKBUTTON) */

#ifndef wxUSE_COMBOCTRL
#       define wxUSE_COMBOCTRL 0
#endif /* !defined(wxUSE_COMBOCTRL) */

#ifndef wxUSE_DATAOBJ
#       define wxUSE_DATAOBJ 0
#endif /* !defined(wxUSE_DATAOBJ) */

#ifndef wxUSE_DATAVIEWCTRL
#       define wxUSE_DATAVIEWCTRL 0
#endif /* !defined(wxUSE_DATAVIEWCTRL) */

#ifndef wxUSE_DATEPICKCTRL
#       define wxUSE_DATEPICKCTRL 0
#endif /* !defined(wxUSE_DATEPICKCTRL) */

#ifndef wxUSE_DC_TRANSFORM_MATRIX
#       define wxUSE_DC_TRANSFORM_MATRIX 1
#endif /* wxUSE_DC_TRANSFORM_MATRIX */

#ifndef wxUSE_DIRPICKERCTRL
#       define wxUSE_DIRPICKERCTRL 0
#endif /* !defined(wxUSE_DIRPICKERCTRL) */

#ifndef wxUSE_DISPLAY
#       define wxUSE_DISPLAY 0
#endif /* !defined(wxUSE_DISPLAY) */

#ifndef wxUSE_FILECTRL
#       define wxUSE_FILECTRL 0
#endif /* !defined(wxUSE_FILECTRL) */

#ifndef wxUSE_FILEDLG
#       define wxUSE_FILEDLG 0
#endif /* !defined(wxUSE_FILEDLG) */

#ifndef wxUSE_FILEPICKERCTRL
#       define wxUSE_FILEPICKERCTRL 0
#endif /* !defined(wxUSE_FILEPICKERCTRL) */

#ifndef wxUSE_FONTDLG
#       define wxUSE_FONTDLG 0
#endif /* !defined(wxUSE_FONTDLG) */

#ifndef wxUSE_FONTMAP
#       define wxUSE_FONTMAP 0
#endif /* !defined(wxUSE_FONTMAP) */

#ifndef wxUSE_FONTPICKERCTRL
#       define wxUSE_FONTPICKERCTRL 0
#endif /* !defined(wxUSE_FONTPICKERCTRL) */

#ifndef wxUSE_GAUGE
#       define wxUSE_GAUGE 0
#endif /* !defined(wxUSE_GAUGE) */

#ifndef wxUSE_GRID
#       define wxUSE_GRID 0
#endif /* !defined(wxUSE_GRID) */

#ifndef wxUSE_HEADERCTRL
#       define wxUSE_HEADERCTRL 0
#endif /* !defined(wxUSE_HEADERCTRL) */

#ifndef wxUSE_HELP
#       define wxUSE_HELP 0
#endif /* !defined(wxUSE_HELP) */

#ifndef wxUSE_HYPERLINKCTRL
#       define wxUSE_HYPERLINKCTRL 0
#endif /* !defined(wxUSE_HYPERLINKCTRL) */

#ifndef wxUSE_HTML
#       define wxUSE_HTML 0
#endif /* !defined(wxUSE_HTML) */

#ifndef wxUSE_LIBMSPACK
#   if !defined(__UNIX__)
        /* set to 0 on platforms that don't have libmspack */
#       define wxUSE_LIBMSPACK 0
#   else
#           define wxUSE_LIBMSPACK 0
#   endif
#endif /* !defined(wxUSE_LIBMSPACK) */

#ifndef wxUSE_ICO_CUR
#       define wxUSE_ICO_CUR 0
#endif /* !defined(wxUSE_ICO_CUR) */

#ifndef wxUSE_IFF
#       define wxUSE_IFF 0
#endif /* !defined(wxUSE_IFF) */

#ifndef wxUSE_IMAGLIST
#       define wxUSE_IMAGLIST 0
#endif /* !defined(wxUSE_IMAGLIST) */

#ifndef wxUSE_INFOBAR
#       define wxUSE_INFOBAR 0
#endif /* !defined(wxUSE_INFOBAR) */

#ifndef wxUSE_LISTBOOK
#       define wxUSE_LISTBOOK 0
#endif /* !defined(wxUSE_LISTBOOK) */

#ifndef wxUSE_LISTBOX
#       define wxUSE_LISTBOX 0
#endif /* !defined(wxUSE_LISTBOX) */

#ifndef wxUSE_LISTCTRL
#       define wxUSE_LISTCTRL 0
#endif /* !defined(wxUSE_LISTCTRL) */

#ifndef wxUSE_MARKUP
#       define wxUSE_MARKUP 0
#endif /* !defined(wxUSE_MARKUP) */

#ifndef wxUSE_MSGDLG
#       define wxUSE_MSGDLG 0
#endif /* !defined(wxUSE_MSGDLG) */

#ifndef wxUSE_ODCOMBOBOX
#       define wxUSE_ODCOMBOBOX 0
#endif /* !defined(wxUSE_ODCOMBOBOX) */

#ifndef wxUSE_PALETTE
#       define wxUSE_PALETTE 0
#endif /* !defined(wxUSE_PALETTE) */

#ifndef wxUSE_PRINTING_ARCHITECTURE
#       define wxUSE_PRINTING_ARCHITECTURE 0
#endif /* !defined(wxUSE_PRINTING_ARCHITECTURE) */

#ifndef wxUSE_RADIOBOX
#       define wxUSE_RADIOBOX 0
#endif /* !defined(wxUSE_RADIOBOX) */

#ifndef wxUSE_RADIOBTN
#       define wxUSE_RADIOBTN 0
#endif /* !defined(wxUSE_RADIOBTN) */

#ifndef wxUSE_REARRANGECTRL
#       define wxUSE_REARRANGECTRL 0
#endif /* !defined(wxUSE_REARRANGECTRL) */

#ifndef wxUSE_RIBBON
#       define wxUSE_RIBBON 0
#endif /* !defined(wxUSE_RIBBON) */

#ifndef wxUSE_RICHMSGDLG
#       define wxUSE_RICHMSGDLG 0
#endif /* !defined(wxUSE_RICHMSGDLG) */

#ifndef wxUSE_SASH
#       define wxUSE_SASH 0
#endif /* !defined(wxUSE_SASH) */

#ifndef wxUSE_SCROLLBAR
#       define wxUSE_SCROLLBAR 0
#endif /* !defined(wxUSE_SCROLLBAR) */

#ifndef wxUSE_SLIDER
#       define wxUSE_SLIDER 0
#endif /* !defined(wxUSE_SLIDER) */

#ifndef wxUSE_SOUND
#       define wxUSE_SOUND 0
#endif /* !defined(wxUSE_SOUND) */

#ifndef wxUSE_SPINBTN
#       define wxUSE_SPINBTN 0
#endif /* !defined(wxUSE_SPINBTN) */

#ifndef wxUSE_SPINCTRL
#       define wxUSE_SPINCTRL 0
#endif /* !defined(wxUSE_SPINCTRL) */

#ifndef wxUSE_SPLASH
#       define wxUSE_SPLASH 0
#endif /* !defined(wxUSE_SPLASH) */

#ifndef wxUSE_SPLITTER
#       define wxUSE_SPLITTER 0
#endif /* !defined(wxUSE_SPLITTER) */

#ifndef wxUSE_STATBMP
#       define wxUSE_STATBMP 0
#endif /* !defined(wxUSE_STATBMP) */

#ifndef wxUSE_STATBOX
#       define wxUSE_STATBOX 0
#endif /* !defined(wxUSE_STATBOX) */

#ifndef wxUSE_STATLINE
#       define wxUSE_STATLINE 0
#endif /* !defined(wxUSE_STATLINE) */

#ifndef wxUSE_STATTEXT
#       define wxUSE_STATTEXT 0
#endif /* !defined(wxUSE_STATTEXT) */

#ifndef wxUSE_STATUSBAR
#       define wxUSE_STATUSBAR 0
#endif /* !defined(wxUSE_STATUSBAR) */

#ifndef wxUSE_TASKBARICON
#       define wxUSE_TASKBARICON 0
#endif /* !defined(wxUSE_TASKBARICON) */

#ifndef wxUSE_TEXTCTRL
#       define wxUSE_TEXTCTRL 0
#endif /* !defined(wxUSE_TEXTCTRL) */

#ifndef wxUSE_TIMEPICKCTRL
#       define wxUSE_TIMEPICKCTRL 0
#endif /* !defined(wxUSE_TIMEPICKCTRL) */

#ifndef wxUSE_TOOLTIPS
#       define wxUSE_TOOLTIPS 0
#endif /* !defined(wxUSE_TOOLTIPS) */

#ifndef wxUSE_TREECTRL
#       define wxUSE_TREECTRL 0
#endif /* !defined(wxUSE_TREECTRL) */

#ifndef wxUSE_TREELISTCTRL
#       define wxUSE_TREELISTCTRL 0
#endif /* !defined(wxUSE_TREELISTCTRL) */

#ifndef wxUSE_UIACTIONSIMULATOR
#       define wxUSE_UIACTIONSIMULATOR 0
#endif /* !defined(wxUSE_UIACTIONSIMULATOR) */

#ifndef wxUSE_VALIDATORS
#       define wxUSE_VALIDATORS 0
#endif /* !defined(wxUSE_VALIDATORS) */

#ifndef wxUSE_WXHTML_HELP
#       define wxUSE_WXHTML_HELP 0
#endif /* !defined(wxUSE_WXHTML_HELP) */

#ifndef wxUSE_XRC
#       define wxUSE_XRC 0
#endif /* !defined(wxUSE_XRC) */

#endif /* wxUSE_GUI */

/*
   Section 2: platform-specific checks.

   This must be done after checking that everything is defined as the platform
   checks use wxUSE_XXX symbols in #if tests.
 */

#if defined(__WXWINCE__)
#  include "wx/msw/wince/chkconf.h"
#elif defined(__WINDOWS__)
#  include "wx/msw/chkconf.h"
#  if defined(__WXGTK__)
#      include "wx/gtk/chkconf.h"
#  endif
#elif defined(__WXGTK__)
#  include "wx/gtk/chkconf.h"
#elif defined(__WXCOCOA__)
#  include "wx/cocoa/chkconf.h"
#elif defined(__WXMAC__)
#  include "wx/osx/chkconf.h"
#elif defined(__OS2__)
#  include "wx/os2/chkconf.h"
#elif defined(__WXDFB__)
#  include "wx/dfb/chkconf.h"
#elif defined(__WXMOTIF__)
#  include "wx/motif/chkconf.h"
#elif defined(__WXX11__)
#  include "wx/x11/chkconf.h"
#elif defined(__WXANDROID__)
#  include "wx/android/chkconf.h"
#endif

/*
    __UNIX__ is also defined under Cygwin but we shouldn't perform these checks
    there if we're building Windows ports.
 */
#if defined(__UNIX__) && !defined(__WINDOWS__)
#   include "wx/unix/chkconf.h"
#endif

#ifdef __WXUNIVERSAL__
#   include "wx/univ/chkconf.h"
#endif

/*
   Section 3a: check consistency of the non-GUI settings.
 */

#if WXWIN_COMPATIBILITY_2_6
#   if !WXWIN_COMPATIBILITY_2_8
#           undef WXWIN_COMPATIBILITY_2_8
#           define WXWIN_COMPATIBILITY_2_8 1
#   endif
#endif /* WXWIN_COMPATIBILITY_2_6 */

#if wxUSE_ARCHIVE_STREAMS
#   if !wxUSE_DATETIME
#           undef wxUSE_ARCHIVE_STREAMS
#           define wxUSE_ARCHIVE_STREAMS 0
#   endif
#endif /* wxUSE_ARCHIVE_STREAMS */

#if wxUSE_PROTOCOL_FILE || wxUSE_PROTOCOL_FTP || wxUSE_PROTOCOL_HTTP
#   if !wxUSE_PROTOCOL
#            undef wxUSE_PROTOCOL
#            define wxUSE_PROTOCOL 1
#   endif
#endif /* wxUSE_PROTOCOL_XXX */

#if wxUSE_URL
#   if !wxUSE_PROTOCOL
#            undef wxUSE_PROTOCOL
#            define wxUSE_PROTOCOL 1
#   endif
#endif /* wxUSE_URL */

#if wxUSE_PROTOCOL
#   if !wxUSE_STREAMS
#           undef wxUSE_STREAMS
#           define wxUSE_STREAMS 1
#   endif
#endif /* wxUSE_PROTOCOL */

/* have to test for wxUSE_HTML before wxUSE_FILESYSTEM */
#if wxUSE_HTML
#   if !wxUSE_FILESYSTEM
#           undef wxUSE_FILESYSTEM
#           define wxUSE_FILESYSTEM 1
#   endif
#endif /* wxUSE_HTML */

#if wxUSE_FS_ARCHIVE
#   if !wxUSE_FILESYSTEM
#           undef wxUSE_FILESYSTEM
#           define wxUSE_FILESYSTEM 1
#   endif
#   if !wxUSE_ARCHIVE_STREAMS
#           undef wxUSE_ARCHIVE_STREAMS
#           define wxUSE_ARCHIVE_STREAMS 1
#   endif
#endif /* wxUSE_FS_ARCHIVE */

#if wxUSE_FILESYSTEM
#   if !wxUSE_STREAMS
#           undef wxUSE_STREAMS
#           define wxUSE_STREAMS 1
#   endif
#   if !wxUSE_FILE && !wxUSE_FFILE
#           undef wxUSE_FILE
#           define wxUSE_FILE 1
#           undef wxUSE_FFILE
#           define wxUSE_FFILE 1
#   endif
#endif /* wxUSE_FILESYSTEM */

#if wxUSE_FS_INET
#   if !wxUSE_PROTOCOL
#           undef wxUSE_PROTOCOL
#           define wxUSE_PROTOCOL 1
#   endif
#endif /* wxUSE_FS_INET */

#if wxUSE_STOPWATCH || wxUSE_DATETIME
#    if !wxUSE_LONGLONG
#            undef wxUSE_LONGLONG
#            define wxUSE_LONGLONG 1
#    endif
#endif /* wxUSE_STOPWATCH */

#if wxUSE_TEXTFILE && !wxUSE_TEXTBUFFER
#       undef wxUSE_TEXTBUFFER
#       define wxUSE_TEXTBUFFER 1
#endif /* wxUSE_TEXTFILE */

#if wxUSE_TEXTFILE && !wxUSE_FILE
#       undef wxUSE_FILE
#       define wxUSE_FILE 1
#endif /* wxUSE_TEXTFILE */

#if !wxUSE_DYNLIB_CLASS
#   if wxUSE_DYNAMIC_LOADER
#           define wxUSE_DYNLIB_CLASS 1
#   endif
#endif  /* wxUSE_DYNLIB_CLASS */

#if wxUSE_ZIPSTREAM
#   if !wxUSE_ZLIB
#           undef wxUSE_ZLIB
#           define wxUSE_ZLIB 1
#   endif
#   if !wxUSE_ARCHIVE_STREAMS
#           undef wxUSE_ARCHIVE_STREAMS
#           define wxUSE_ARCHIVE_STREAMS 1
#   endif
#endif /* wxUSE_ZIPSTREAM */

#if wxUSE_TARSTREAM
#   if !wxUSE_ARCHIVE_STREAMS
#           undef wxUSE_ARCHIVE_STREAMS
#           define wxUSE_ARCHIVE_STREAMS 1
#   endif
#endif /* wxUSE_TARSTREAM */

/*
   Section 3b: the tests for the GUI settings only.
 */
#if wxUSE_GUI

#if wxUSE_ACCESSIBILITY && !defined(__WXMSW__)
#       undef wxUSE_ACCESSIBILITY
#       define wxUSE_ACCESSIBILITY 0
#endif /* wxUSE_ACCESSIBILITY */

#if wxUSE_BUTTON || \
    wxUSE_CALENDARCTRL || \
    wxUSE_CARET || \
    wxUSE_COMBOBOX || \
    wxUSE_BMPBUTTON || \
    wxUSE_CHECKBOX || \
    wxUSE_CHECKLISTBOX || \
    wxUSE_CHOICE || \
    wxUSE_GAUGE || \
    wxUSE_GRID || \
    wxUSE_HEADERCTRL || \
    wxUSE_LISTBOX || \
    wxUSE_LISTCTRL || \
    wxUSE_RADIOBOX || \
    wxUSE_RADIOBTN || \
    wxUSE_REARRANGECTRL || \
    wxUSE_SCROLLBAR || \
    wxUSE_SLIDER || \
    wxUSE_SPINBTN || \
    wxUSE_SPINCTRL || \
    wxUSE_STATBMP || \
    wxUSE_STATBOX || \
    wxUSE_STATLINE || \
    wxUSE_STATTEXT || \
    wxUSE_STATUSBAR || \
    wxUSE_TEXTCTRL || \
    wxUSE_TREECTRL || \
    wxUSE_TREELISTCTRL
#    if !wxUSE_CONTROLS
#            undef wxUSE_CONTROLS
#            define wxUSE_CONTROLS 1
#    endif
#endif /* controls */

#if wxUSE_BMPBUTTON
#    if !wxUSE_BUTTON
#            undef wxUSE_BUTTON
#            define wxUSE_BUTTON 1
#    endif
#endif /* wxUSE_BMPBUTTON */

#if wxUSE_COMMANDLINKBUTTON
#    if !wxUSE_BUTTON
#            undef wxUSE_BUTTON
#            define wxUSE_BUTTON 1
#    endif
#endif /* wxUSE_COMMANDLINKBUTTON */

/*
   wxUSE_BOOKCTRL should be only used if any of the controls deriving from it
   are used
 */
#ifdef wxUSE_BOOKCTRL
#       undef wxUSE_BOOKCTRL
#endif

#define wxUSE_BOOKCTRL ( \
                        wxUSE_LISTBOOK || \
                        wxUSE_CHOICEBOOK || \
                        wxUSE_TOOLBOOK || \
                        wxUSE_TREEBOOK)

#if wxUSE_COLLPANE
#   if !wxUSE_BUTTON || !wxUSE_STATLINE
#           undef wxUSE_COLLPANE
#           define wxUSE_COLLPANE 0
#   endif
#endif /* wxUSE_COLLPANE */

#if wxUSE_LISTBOOK
#   if !wxUSE_LISTCTRL
#           undef wxUSE_LISTCTRL
#           define wxUSE_LISTCTRL 1
#   endif
#endif /* wxUSE_LISTBOOK */

#if wxUSE_CHOICEBOOK
#   if !wxUSE_CHOICE
#           undef wxUSE_CHOICE
#           define wxUSE_CHOICE 1
#   endif
#endif /* wxUSE_CHOICEBOOK */

#if !wxUSE_ODCOMBOBOX
#   if wxUSE_BITMAPCOMBOBOX
#           undef wxUSE_BITMAPCOMBOBOX
#           define wxUSE_BITMAPCOMBOBOX 0
#   endif
#endif /* !wxUSE_ODCOMBOBOX */

#if !wxUSE_HEADERCTRL
#   if wxUSE_DATAVIEWCTRL || wxUSE_GRID
#           undef wxUSE_HEADERCTRL
#           define wxUSE_HEADERCTRL 1
#   endif
#endif /* !wxUSE_HEADERCTRL */

#if wxUSE_REARRANGECTRL
#   if !wxUSE_CHECKLISTBOX
#           undef wxUSE_REARRANGECTRL
#           define wxUSE_REARRANGECTRL 0
#   endif
#endif /* wxUSE_REARRANGECTRL */

#if wxUSE_RICHMSGDLG
#    if !wxUSE_MSGDLG
#            undef wxUSE_MSGDLG
#            define wxUSE_MSGDLG 1
#    endif
#endif /* wxUSE_RICHMSGDLG */

/* don't attempt to use native status bar on the platforms not having it */
#ifndef wxUSE_NATIVE_STATUSBAR
#   define wxUSE_NATIVE_STATUSBAR 0
#elif wxUSE_NATIVE_STATUSBAR
#   if defined(__WXUNIVERSAL__) || !(defined(__WXMSW__) || defined(__WXMAC__))
#       undef wxUSE_NATIVE_STATUSBAR
#       define wxUSE_NATIVE_STATUSBAR 0
#   endif
#endif

/* generic controls dependencies */
#if !defined(__WXMSW__) || defined(__WXUNIVERSAL__)
#   if wxUSE_FONTDLG || wxUSE_FILEDLG || wxUSE_CHOICEDLG
        /* all common controls are needed by these dialogs */
#       if !defined(wxUSE_CHOICE) || \
           !defined(wxUSE_TEXTCTRL) || \
           !defined(wxUSE_BUTTON) || \
           !defined(wxUSE_CHECKBOX) || \
           !defined(wxUSE_STATTEXT)
#               undef wxUSE_CHOICE
#               define wxUSE_CHOICE 1
#               undef wxUSE_TEXTCTRL
#               define wxUSE_TEXTCTRL 1
#               undef wxUSE_BUTTON
#               define wxUSE_BUTTON 1
#               undef wxUSE_CHECKBOX
#               define wxUSE_CHECKBOX 1
#               undef wxUSE_STATTEXT
#               define wxUSE_STATTEXT 1
#       endif
#   endif
#endif /* !wxMSW || wxUniv */

/* generic file dialog depends on (generic) file control */
#if wxUSE_FILEDLG && !wxUSE_FILECTRL && \
        (defined(__WXUNIVERSAL__) || defined(__WXGTK__))
#       undef wxUSE_FILECTRL
#       define wxUSE_FILECTRL 1
#endif /* wxUSE_FILEDLG */

/* common dependencies */
#if wxUSE_ARTPROVIDER_TANGO
#   if !(wxUSE_STREAMS && wxUSE_IMAGE && wxUSE_LIBPNG)
#           undef wxUSE_ARTPROVIDER_TANGO
#           define wxUSE_ARTPROVIDER_TANGO 0
#   endif
#endif /* wxUSE_ARTPROVIDER_TANGO */

#if wxUSE_CALENDARCTRL
#   if !(wxUSE_SPINBTN && wxUSE_COMBOBOX)
#           undef wxUSE_SPINBTN
#           undef wxUSE_COMBOBOX
#           define wxUSE_SPINBTN 1
#           define wxUSE_COMBOBOX 1
#   endif

#   if !wxUSE_DATETIME
#           undef wxUSE_DATETIME
#           define wxUSE_DATETIME 1
#   endif
#endif /* wxUSE_CALENDARCTRL */

#if wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL
#   if !wxUSE_DATETIME
#           undef wxUSE_DATETIME
#           define wxUSE_DATETIME 1
#   endif
#endif /* wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL */

#if wxUSE_CHECKLISTBOX
#   if !wxUSE_LISTBOX
#            undef wxUSE_LISTBOX
#            define wxUSE_LISTBOX 1
#   endif
#endif /* wxUSE_CHECKLISTBOX */

#if wxUSE_CHOICEDLG
#   if !wxUSE_LISTBOX
#            undef wxUSE_LISTBOX
#            define wxUSE_LISTBOX 1
#   endif
#endif /* wxUSE_CHOICEDLG */

#if wxUSE_FILECTRL
#   if !wxUSE_DATETIME
#           undef wxUSE_DATETIME
#           define wxUSE_DATETIME 1
#   endif
#endif /* wxUSE_FILECTRL */

#if wxUSE_HELP
#   if !wxUSE_BMPBUTTON
#           undef wxUSE_BMPBUTTON
#           define wxUSE_BMPBUTTON 1
#   endif

#   if !wxUSE_CHOICEDLG
#           undef wxUSE_CHOICEDLG
#           define wxUSE_CHOICEDLG 1
#   endif
#endif /* wxUSE_HELP */

#if wxUSE_MS_HTML_HELP
    /*
        this doesn't make sense for platforms other than MSW but we still
        define it in wx/setup_inc.h so don't complain if it happens to be
        defined under another platform but just silently fix it.
     */
#   ifndef __WXMSW__
#       undef wxUSE_MS_HTML_HELP
#       define wxUSE_MS_HTML_HELP 0
#   endif
#endif /* wxUSE_MS_HTML_HELP */

#if wxUSE_WXHTML_HELP
#   if !wxUSE_HELP || !wxUSE_HTML || !wxUSE_COMBOBOX || !wxUSE_SPINCTRL
#           undef wxUSE_HELP
#           define wxUSE_HELP 1
#           undef wxUSE_HTML
#           define wxUSE_HTML 1
#           undef wxUSE_COMBOBOX
#           define wxUSE_COMBOBOX 1
#           undef wxUSE_SPINCTRL
#           define wxUSE_SPINCTRL 1
#   endif
#endif /* wxUSE_WXHTML_HELP */

#if !wxUSE_IMAGE
/*
   The default wxUSE_IMAGE setting is 1, so if it's set to 0 we assume the
   user explicitly wants this and disable all other features that require
   wxUSE_IMAGE.
 */
#   if wxUSE_DRAGIMAGE
#            undef wxUSE_DRAGIMAGE
#            define wxUSE_DRAGIMAGE 0
#   endif

#   if wxUSE_LIBPNG
#            undef wxUSE_LIBPNG
#            define wxUSE_LIBPNG 0
#   endif

#   if wxUSE_LIBJPEG
#            undef wxUSE_LIBJPEG
#            define wxUSE_LIBJPEG 0
#   endif

#   if wxUSE_LIBTIFF
#            undef wxUSE_LIBTIFF
#            define wxUSE_LIBTIFF 0
#   endif

#   if wxUSE_GIF
#            undef wxUSE_GIF
#            define wxUSE_GIF 0
#   endif

#   if wxUSE_PNM
#            undef wxUSE_PNM
#            define wxUSE_PNM 0
#   endif

#   if wxUSE_PCX
#            undef wxUSE_PCX
#            define wxUSE_PCX 0
#   endif

#   if wxUSE_IFF
#            undef wxUSE_IFF
#            define wxUSE_IFF 0
#   endif

#   if wxUSE_XPM
#            undef wxUSE_XPM
#            define wxUSE_XPM 0
#   endif

#endif /* !wxUSE_IMAGE */

#if wxUSE_PRINTING_ARCHITECTURE
#   if !wxUSE_COMBOBOX
#           undef wxUSE_COMBOBOX
#           define wxUSE_COMBOBOX 1
#   endif
#endif /* wxUSE_PRINTING_ARCHITECTURE */

#if !wxUSE_GAUGE || !wxUSE_BUTTON
#   if wxUSE_PROGRESSDLG
#           undef wxUSE_GAUGE
#           undef wxUSE_BUTTON
#           define wxUSE_GAUGE 1
#           define wxUSE_BUTTON 1
#   endif
#endif /* !wxUSE_GAUGE */

#if !wxUSE_BUTTON
#   if wxUSE_FONTDLG || \
       wxUSE_FILEDLG || \
       wxUSE_CHOICEDLG || \
       wxUSE_NUMBERDLG || \
       wxUSE_TEXTDLG || \
       wxUSE_DIRDLG || \
       wxUSE_STARTUP_TIPS || \
       wxUSE_WIZARDDLG
#           undef wxUSE_BUTTON
#           define wxUSE_BUTTON 1
#   endif
#endif /* !wxUSE_BUTTON */

#if !wxUSE_IMAGLIST
#   if wxUSE_TREECTRL || wxUSE_LISTCTRL || wxUSE_TREELISTCTRL
#           undef wxUSE_IMAGLIST
#           define wxUSE_IMAGLIST 1
#   endif
#endif /* !wxUSE_IMAGLIST */

#if wxUSE_RADIOBOX
#   if !wxUSE_RADIOBTN
#            undef wxUSE_RADIOBTN
#            define wxUSE_RADIOBTN 1
#   endif
#   if !wxUSE_STATBOX
#            undef wxUSE_STATBOX
#            define wxUSE_STATBOX 1
#   endif
#endif /* wxUSE_RADIOBOX */

#if wxUSE_CLIPBOARD && !wxUSE_DATAOBJ
#       undef wxUSE_DATAOBJ
#       define wxUSE_DATAOBJ 1
#endif /* wxUSE_CLIPBOARD */

#if wxUSE_XRC && !wxUSE_XML
#       undef wxUSE_XRC
#       define wxUSE_XRC 0
#endif /* wxUSE_XRC */

#if wxUSE_SVG && !wxUSE_STREAMS
#       undef wxUSE_SVG
#       define wxUSE_SVG 0
#endif /* wxUSE_SVG */

#if wxUSE_SVG && !wxUSE_IMAGE
#       undef wxUSE_SVG
#       define wxUSE_SVG 0
#endif /* wxUSE_SVG */

#if wxUSE_SVG && !wxUSE_LIBPNG
#       undef wxUSE_SVG
#       define wxUSE_SVG 0
#endif /* wxUSE_SVG */

#if wxUSE_TASKBARICON
#       undef wxUSE_TASKBARICON
#       define wxUSE_TASKBARICON 0
#endif /* wxUSE_TASKBARICON */

#if !wxUSE_VARIANT
#   if wxUSE_DATAVIEWCTRL
#           undef wxUSE_DATAVIEWCTRL
#           define wxUSE_DATAVIEWCTRL 0
#   endif
#endif /* wxUSE_VARIANT */

#if wxUSE_TREELISTCTRL && !wxUSE_DATAVIEWCTRL
#       undef wxUSE_TREELISTCTRL
#       define wxUSE_TREELISTCTRL 0
#endif /* wxUSE_TREELISTCTRL */

#endif /* wxUSE_GUI */

#endif /* _WX_CHKCONF_H_ */
