/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/setup.h
// Purpose:     Configuration for the library
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SETUP_H_
#define _WX_SETUP_H_

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
#if (defined(_MSC_VER) && _MSC_VER < 1200)
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

// use wxTextBuffer class: required by wxTextFile
#define wxUSE_TEXTBUFFER    1

// use wxTextFile class: requires wxFile and wxTextBuffer, required by
// wxFileConfig
#define wxUSE_TEXTFILE      1

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
// wxUniversal-only options
// ----------------------------------------------------------------------------

/* --- end common options --- */

#endif // _WX_SETUP_H_

