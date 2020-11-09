/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/wince/setup.h
// Purpose:     Configuration for the library
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SETUP_H_
#define _WX_SETUP_H_

/* --- start common options --- */
// ----------------------------------------------------------------------------
// compatibility settings
// ----------------------------------------------------------------------------

// This setting determines the compatibility with 2.6 API: set it to 0 to
// flag all cases of using deprecated functions.
//
// Default is 1 but please try building your code with 0 as the default will
// change to 0 in the next version and the deprecated functions will disappear
// in the version after it completely.
//
// Recommended setting: 0 (please update your code)
#define WXWIN_COMPATIBILITY_2_6 0

// This setting determines the compatibility with 2.8 API: set it to 0 to
// flag all cases of using deprecated functions.
//
// Default is 1 but please try building your code with 0 as the default will
// change to 0 in the next version and the deprecated functions will disappear
// in the version after it completely.
//
// Recommended setting: 0 (please update your code)
#define WXWIN_COMPATIBILITY_2_8 1

// MSW-only: Set to 0 for accurate dialog units, else 1 for old behaviour when
// default system font is used for wxWindow::GetCharWidth/Height() instead of
// the current font.
//
// Default is 0
//
// Recommended setting: 0
#define wxDIALOG_UNIT_COMPATIBILITY   0

// ----------------------------------------------------------------------------
// debugging settings
// ----------------------------------------------------------------------------

// wxDEBUG_LEVEL will be defined as 1 in wx/debug.h so normally there is no
// need to define it here. You may do it for two reasons: either completely
// disable/compile out the asserts in release version (then do it inside #ifdef
// NDEBUG) or, on the contrary, enable more asserts, including the usually
// disabled ones, in the debug build (then do it inside #ifndef NDEBUG)
//
// #ifdef NDEBUG
//  #define wxDEBUG_LEVEL 0
// #else
//  #define wxDEBUG_LEVEL 2
// #endif

// wxHandleFatalExceptions() may be used to catch the program faults at run
// time and, instead of terminating the program with a usual GPF message box,
// call the user-defined wxApp::OnFatalException() function. If you set
// wxUSE_ON_FATAL_EXCEPTION to 0, wxHandleFatalExceptions() will not work.
//
// This setting is for Win32 only and can only be enabled if your compiler
// supports Win32 structured exception handling (currently only VC++ does)
//
// Default is 1
//
// Recommended setting: 1 if your compiler supports it.
#define wxUSE_ON_FATAL_EXCEPTION 1

// Generic comment about debugging settings: they are very useful if you don't
// use any other memory leak detection tools such as Purify/BoundsChecker, but
// are probably redundant otherwise. Also, Visual C++ CRT has the same features
// as wxWidgets memory debugging subsystem built in since version 5.0 and you
// may prefer to use it instead of built in memory debugging code because it is
// faster and more fool proof.
//
// Using VC++ CRT memory debugging is enabled by default in debug build (_DEBUG
// is defined) if wxUSE_GLOBAL_MEMORY_OPERATORS is *not* enabled (i.e. is 0)
// and if __NO_VC_CRTDBG__ is not defined.

// The rest of the options in this section are obsolete and not supported,
// enable them at your own risk.

// ----------------------------------------------------------------------------
// Unicode support
// ----------------------------------------------------------------------------

// These settings are obsolete: the library is always built in Unicode mode
// now, only set wxUSE_UNICODE to 0 to compile legacy code in ANSI mode if
// absolutely necessary -- updating it is strongly recommended as the ANSI mode
// will disappear completely in future wxWidgets releases.
#ifndef wxUSE_UNICODE
    #define wxUSE_UNICODE 1
#endif

// wxUSE_WCHAR_T is required by wxWidgets now, don't change.
#define wxUSE_WCHAR_T 1

// ----------------------------------------------------------------------------
// global features
// ----------------------------------------------------------------------------

// Support for multithreaded applications: if 1, compile in thread classes
// (thread.h) and make the library a bit more thread safe. Although thread
// support is quite stable by now, you may still consider recompiling the
// library without it if you have no use for it - this will result in a
// somewhat smaller and faster operation.
//
// Notice that if wxNO_THREADS is defined, wxUSE_THREADS is automatically reset
// to 0 in wx/chkconf.h, so, for example, if you set USE_THREADS to 0 in
// build/msw/config.* file this value will have no effect.
//
// Default is 1
//
// Recommended setting: 0 unless you do plan to develop MT applications
#define wxUSE_THREADS 1

// If enabled, compiles wxWidgets streams classes
//
// wx stream classes are used for image IO, process IO redirection, network
// protocols implementation and much more and so disabling this results in a
// lot of other functionality being lost.
//
// Default is 1
//
// Recommended setting: 1 as setting it to 0 disables many other things
#define wxUSE_STREAMS       1

// Support for positional parameters (e.g. %1$d, %2$s ...) in wxVsnprintf.
// Note that if the system's implementation does not support positional
// parameters, setting this to 1 forces the use of the wxWidgets implementation
// of wxVsnprintf. The standard vsnprintf() supports positional parameters on
// many Unix systems but usually doesn't under Windows.
//
// Positional parameters are very useful when translating a program since using
// them in formatting strings allow translators to correctly reorder the
// translated sentences.
//
// Default is 1
//
// Recommended setting: 1 if you want to support multiple languages
#define wxUSE_PRINTF_POS_PARAMS      1

// Enable the use of compiler-specific thread local storage keyword, if any.
// This is used for wxTLS_XXX() macros implementation and normally should use
// the compiler-provided support as it's simpler and more efficient, but is
// disabled under Windows in wx/msw/chkconf.h as it can't be used if wxWidgets
// is used in a dynamically loaded Win32 DLL (i.e. using LoadLibrary()) under
// XP as this triggers a bug in compiler TLS support that results in crashes
// when any TLS variables are used.
//
// If you're absolutely sure that your build of wxWidgets is never going to be
// used in such situation, either because it's not going to be linked from any
// kind of plugin or because you only target Vista or later systems, you can
// set this to 2 to force the use of compiler TLS even under MSW.
//
// Default is 1 meaning that compiler TLS is used only if it's 100% safe.
//
// Recommended setting: 2 if you want to have maximal performance and don't
// care about the scenario described above.
#define wxUSE_COMPILER_TLS 1

// ----------------------------------------------------------------------------
// Interoperability with the standard library.
// ----------------------------------------------------------------------------

// Set wxUSE_STL to 1 to enable maximal interoperability with the standard
// library, even at the cost of backwards compatibility.
//
// Default is 0
//
// Recommended setting: 0 as the options below already provide a relatively
// good level of interoperability and changing this option arguably isn't worth
// diverging from the official builds of the library.
#define wxUSE_STL 0

// This is not a real option but is used as the default value for
// wxUSE_STD_IOSTREAM, wxUSE_STD_STRING and wxUSE_STD_CONTAINERS.
//
// Currently the Digital Mars and Watcom compilers come without standard C++
// library headers by default, wxUSE_STD_STRING can be set to 1 if you do have
// them (e.g. from STLPort).
//
// VC++ 5.0 does include standard C++ library headers, however they produce
// many warnings that can't be turned off when compiled at warning level 4.
#if defined(__DMC__) || defined(__WATCOMC__) \
        || (defined(_MSC_VER) && _MSC_VER < 1200)
    #define wxUSE_STD_DEFAULT  0
#else
    #define wxUSE_STD_DEFAULT  1
#endif

// Use standard C++ containers to implement wxVector<>, wxStack<>, wxDList<>
// and wxHashXXX<> classes. If disabled, wxWidgets own (mostly compatible but
// usually more limited) implementations are used which allows to avoid the
// dependency on the C++ run-time library.
//
// Notice that the compilers mentioned in wxUSE_STD_DEFAULT comment above don't
// support using standard containers and that VC6 needs non-default options for
// such build to avoid getting "fatal error C1076: compiler limit : internal
// heap limit reached; use /Zm to specify a higher limit" in its own standard
// headers, so you need to ensure you do increase the heap size before enabling
// this option for this compiler.
//
// Default is 0 for compatibility reasons.
//
// Recommended setting: 1 unless compatibility with the official wxWidgets
// build and/or the existing code is a concern.
#define wxUSE_STD_CONTAINERS 0

// Use standard C++ streams if 1 instead of wx streams in some places. If
// disabled, wx streams are used everywhere and wxWidgets doesn't depend on the
// standard streams library.
//
// Notice that enabling this does not replace wx streams with std streams
// everywhere, in a lot of places wx streams are used no matter what.
//
// Default is 1 if compiler supports it.
//
// Recommended setting: 1 if you use the standard streams anyhow and so
//                      dependency on the standard streams library is not a
//                      problem
#define wxUSE_STD_IOSTREAM  wxUSE_STD_DEFAULT

// Enable minimal interoperability with the standard C++ string class if 1.
// "Minimal" means that wxString can be constructed from std::string or
// std::wstring but can't be implicitly converted to them. You need to enable
// the option below for the latter.
//
// Default is 1 for most compilers.
//
// Recommended setting: 1 unless you want to ensure your program doesn't use
//                      the standard C++ library at all.
#define wxUSE_STD_STRING  wxUSE_STD_DEFAULT

// Make wxString as much interchangeable with std::[w]string as possible, in
// particular allow implicit conversion of wxString to either of these classes.
// This comes at a price (or a benefit, depending on your point of view) of not
// allowing implicit conversion to "const char *" and "const wchar_t *".
//
// Because a lot of existing code relies on these conversions, this option is
// disabled by default but can be enabled for your build if you don't care
// about compatibility.
//
// Default is 0 if wxUSE_STL has its default value or 1 if it is enabled.
//
// Recommended setting: 0 to remain compatible with the official builds of
// wxWidgets.
#define wxUSE_STD_STRING_CONV_IN_WXSTRING wxUSE_STL

// VC++ 4.2 and above allows <iostream> and <iostream.h> but you can't mix
// them. Set this option to 1 to use <iostream.h>, 0 to use <iostream>.
//
// Note that newer compilers (including VC++ 7.1 and later) don't support
// wxUSE_IOSTREAMH == 1 and so <iostream> will be used anyhow.
//
// Default is 0.
//
// Recommended setting: 0, only set to 1 if you use a really old compiler
#define wxUSE_IOSTREAMH     0


// ----------------------------------------------------------------------------
// non GUI features selection
// ----------------------------------------------------------------------------

// Set wxUSE_LONGLONG to 1 to compile the wxLongLong class. This is a 64 bit
// integer which is implemented in terms of native 64 bit integers if any or
// uses emulation otherwise.
//
// This class is required by wxDateTime and so you should enable it if you want
// to use wxDateTime. For most modern platforms, it will use the native 64 bit
// integers in which case (almost) all of its functions are inline and it
// almost does not take any space, so there should be no reason to switch it
// off.
//
// Recommended setting: 1
#define wxUSE_LONGLONG      1

// Set this to 1 to be able to use wxEventLoop even in console applications
// (i.e. using base library only, without GUI). This is mostly useful for
// processing socket events but is also necessary to use timers in console
// applications
//
// Default is 1.
//
// Recommended setting: 1 (but can be safely disabled if you don't use it)
#define wxUSE_CONSOLE_EVENTLOOP 1

// Set wxUSE_(F)FILE to 1 to compile wx(F)File classes. wxFile uses low level
// POSIX functions for file access, wxFFile uses ANSI C stdio.h functions.
//
// Default is 1
//
// Recommended setting: 1 (wxFile is highly recommended as it is required by
// i18n code, wxFileConfig and others)
#define wxUSE_FILE          1
#define wxUSE_FFILE         1

// Use wxStandardPaths class which allows to retrieve some standard locations
// in the file system
//
// Default is 1
//
// Recommended setting: 1 (may be disabled to save space, but not much)
#define wxUSE_STDPATHS      1

// use wxTextBuffer class: required by wxTextFile
#define wxUSE_TEXTBUFFER    1

// use wxTextFile class: requires wxFile and wxTextBuffer, required by
// wxFileConfig
#define wxUSE_TEXTFILE      1

// i18n support: _() macro, wxLocale class. Requires wxTextFile.
#define wxUSE_INTL          1

// Provide wxFoo_l() functions similar to standard foo() functions but taking
// an extra locale parameter.
//
// Notice that this is fully implemented only for the systems providing POSIX
// xlocale support or Microsoft Visual C++ >= 8 (which provides proprietary
// almost-equivalent of xlocale functions), otherwise wxFoo_l() functions will
// only work for the current user locale and "C" locale. You can use
// wxHAS_XLOCALE_SUPPORT to test whether the full support is available.
//
// Default is 1
//
// Recommended setting: 1 but may be disabled if you are writing programs
// running only in C locale anyhow
#define wxUSE_XLOCALE       1

// Set wxUSE_DATETIME to 1 to compile the wxDateTime and related classes which
// allow to manipulate dates, times and time intervals. wxDateTime replaces the
// old wxTime and wxDate classes which are still provided for backwards
// compatibility (and implemented in terms of wxDateTime).
//
// Note that this class is relatively new and is still officially in alpha
// stage because some features are not yet (fully) implemented. It is already
// quite useful though and should only be disabled if you are aiming at
// absolutely minimal version of the library.
//
// Requires: wxUSE_LONGLONG
//
// Default is 1
//
// Recommended setting: 1
#define wxUSE_DATETIME      1

// Set wxUSE_TIMER to 1 to compile wxTimer class
//
// Default is 1
//
// Recommended setting: 1
#define wxUSE_TIMER         1

// Setting wxUSE_CONFIG to 1 enables the use of wxConfig and related classes
// which allow the application to store its settings in the persistent
// storage. Setting this to 1 will also enable on-demand creation of the
// global config object in wxApp.
//
// See also wxUSE_CONFIG_NATIVE below.
//
// Recommended setting: 1
#define wxUSE_CONFIG        1

// If wxUSE_CONFIG is 1, you may choose to use either the native config
// classes under Windows (using .INI files under Win16 and the registry under
// Win32) or the portable text file format used by the config classes under
// Unix.
//
// Default is 1 to use native classes. Note that you may still use
// wxFileConfig even if you set this to 1 - just the config object created by
// default for the applications needs will be a wxRegConfig or wxIniConfig and
// not wxFileConfig.
//
// Recommended setting: 1
#define wxUSE_CONFIG_NATIVE   1

// Compile in classes for run-time DLL loading and function calling.
// Required by wxUSE_DIALUP_MANAGER.
//
// This setting is for Win32 only
//
// Default is 1.
//
// Recommended setting: 1
#define wxUSE_DYNLIB_CLASS    1

// experimental, don't use for now
#define wxUSE_DYNAMIC_LOADER  1

// Set to 1 to use ipv6 socket classes (requires wxUSE_SOCKETS)
//
// Notice that currently setting this option under Windows will result in
// programs which can only run on recent OS versions (with ws2_32.dll
// installed) which is why it is disabled by default.
//
// Default is 1.
//
// Recommended setting: 1 if you need IPv6 support
#define wxUSE_IPV6          0

// Set to 1 to enable virtual file systems (required by wxHTML)
#define wxUSE_FILESYSTEM    1

// Set to 1 to enable virtual ZIP filesystem (requires wxUSE_FILESYSTEM)
#define wxUSE_FS_ZIP        1

// Set to 1 to enable virtual archive filesystem (requires wxUSE_FILESYSTEM)
#define wxUSE_FS_ARCHIVE    1

// Set to 1 to enable virtual Internet filesystem (requires wxUSE_FILESYSTEM)
#define wxUSE_FS_INET       1

// wxArchive classes for accessing archives such as zip and tar
#define wxUSE_ARCHIVE_STREAMS     1

// Set to 1 to compile wxZipInput/OutputStream classes.
#define wxUSE_ZIPSTREAM     1

// Set to 1 to compile wxTarInput/OutputStream classes.
#define wxUSE_TARSTREAM     1

// Set to 1 to compile wxZlibInput/OutputStream classes. Also required by
// wxUSE_LIBPNG
#define wxUSE_ZLIB          1

// If enabled, the code written by Apple will be used to write, in a portable
// way, float on the disk. See extended.c for the license which is different
// from wxWidgets one.
//
// Default is 1.
//
// Recommended setting: 1 unless you don't like the license terms (unlikely)
#define wxUSE_APPLE_IEEE          1

// wxProtocol and related classes: if you want to use either of wxFTP, wxHTTP
// or wxURL you need to set this to 1.
//
// Default is 1.
//
// Recommended setting: 1
#define wxUSE_PROTOCOL 1

// The settings for the individual URL schemes
#define wxUSE_PROTOCOL_FILE 1
#define wxUSE_PROTOCOL_FTP 1
#define wxUSE_PROTOCOL_HTTP 1

// ----------------------------------------------------------------------------
// Individual GUI controls
// ----------------------------------------------------------------------------

// Each of the settings below corresponds to one wxWidgets control. They are
// all switched on by default but may be disabled if you are sure that your
// program (including any standard dialogs it can show!) doesn't need them and
// if you desperately want to save some space. If you use any of these you must
// set wxUSE_CONTROLS as well.
//
// Default is 1
//
// Recommended setting: 1
#define wxUSE_BANNERWINDOW  1   // wxBannerWindow
#define wxUSE_BMPBUTTON     1   // wxBitmapButton
#define wxUSE_CALENDARCTRL  1   // wxCalendarCtrl
#define wxUSE_CHECKBOX      1   // wxCheckBox
#define wxUSE_CHOICE        1   // wxChoice
#define wxUSE_COLLPANE      1   // wxCollapsiblePane
#define wxUSE_COMBOBOX      1   // wxComboBox
#define wxUSE_COMMANDLINKBUTTON 1   // wxCommandLinkButton
#define wxUSE_EDITABLELISTBOX 1 // wxEditableListBox
#define wxUSE_FILECTRL      1   // wxFileCtrl
#define wxUSE_FONTPICKERCTRL 1  // wxFontPickerCtrl
#define wxUSE_GAUGE         1   // wxGauge
#define wxUSE_HEADERCTRL    1   // wxHeaderCtrl
#define wxUSE_HYPERLINKCTRL 1   // wxHyperlinkCtrl
#define wxUSE_LISTBOX       1   // wxListBox
#define wxUSE_LISTCTRL      1   // wxListCtrl
#define wxUSE_RADIOBOX      1   // wxRadioBox
#define wxUSE_RADIOBTN      1   // wxRadioButton
#define wxUSE_RICHMSGDLG    1   // wxRichMessageDialog
#define wxUSE_SCROLLBAR     1   // wxScrollBar
#define wxUSE_SEARCHCTRL    1   // wxSearchCtrl
#define wxUSE_SLIDER        1   // wxSlider
#define wxUSE_SPINBTN       1   // wxSpinButton
#define wxUSE_SPINCTRL      1   // wxSpinCtrl
#define wxUSE_STATBOX       1   // wxStaticBox
#define wxUSE_STATLINE      1   // wxStaticLine
#define wxUSE_STATTEXT      1   // wxStaticText
#define wxUSE_STATBMP       1   // wxStaticBitmap
#define wxUSE_TEXTCTRL      1   // wxTextCtrl

// ----------------------------------------------------------------------------
// Miscellaneous GUI stuff
// ----------------------------------------------------------------------------

// Use reference counted ID management: this means that wxWidgets will track
// the automatically allocated ids (those used when you use wxID_ANY when
// creating a window, menu or toolbar item &c) instead of just supposing that
// the program never runs out of them. This is mostly useful only under wxMSW
// where the total ids range is limited to SHRT_MIN..SHRT_MAX and where
// long-running programs can run into problems with ids reuse without this. On
// the other platforms, where the ids have the full int range, this shouldn't
// be necessary.
#ifdef __WXMSW__
#define wxUSE_AUTOID_MANAGEMENT 1
#else
#define wxUSE_AUTOID_MANAGEMENT 0
#endif

// ----------------------------------------------------------------------------
// Metafiles support
// ----------------------------------------------------------------------------

// Windows supports the graphics format known as metafile which is, though not
// portable, is widely used under Windows and so is supported by wxWin (under
// Windows only, of course). Win16 (Win3.1) used the so-called "Window
// MetaFiles" or WMFs which were replaced with "Enhanced MetaFiles" or EMFs in
// Win32 (Win9x, NT, 2000). Both of these are supported in wxWin and, by
// default, WMFs will be used under Win16 and EMFs under Win32. This may be
// changed by setting wxUSE_WIN_METAFILES_ALWAYS to 1 and/or setting
// wxUSE_ENH_METAFILE to 0. You may also set wxUSE_METAFILE to 0 to not compile
// in any metafile related classes at all.
//
// Default is 1 for wxUSE_ENH_METAFILE and 0 for wxUSE_WIN_METAFILES_ALWAYS.
//
// Recommended setting: default or 0 for everything for portable programs.
#define wxUSE_WIN_METAFILES_ALWAYS  0

// ----------------------------------------------------------------------------
// image format support
// ----------------------------------------------------------------------------

// wxImage supports many different image formats which can be configured at
// compile-time. BMP is always supported, others are optional and can be safely
// disabled if you don't plan to use images in such format sometimes saving
// substantial amount of code in the final library.
//
// Some formats require an extra library which is included in wxWin sources
// which is mentioned if it is the case.

// Set to 1 for PNG format support (requires libpng). Also requires wxUSE_ZLIB.
#define wxUSE_LIBPNG        1

// Set to 1 for JPEG format support (requires libjpeg)
#define wxUSE_LIBJPEG       1

// Set to 1 for TIFF format support (requires libtiff)
#define wxUSE_LIBTIFF       1

// Set to 1 for TGA format support (loading only)
#define wxUSE_TGA           1

// Set to 1 for PNM format support
#define wxUSE_PNM           1

// Set to 1 for PCX format support
#define wxUSE_PCX           1

// Set to 1 for IFF format support (Amiga format)
#define wxUSE_IFF           0

/* --- end common options --- */

// ----------------------------------------------------------------------------
// general Windows-specific stuff
// ----------------------------------------------------------------------------

// Set this to 1 to enable wxDIB (don't change unless you have reason to)
#define wxUSE_WXDIB 1

// Set this to 1 to compile in wxRegKey class.
//
// Default is 1
//
// Recommended setting: 1, this is used internally by wx in a few places
#define wxUSE_REGKEY 1

#endif // _WX_SETUP_H_
