/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/main.cpp
// Purpose:     WinMain/DllMain
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
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

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/event.h"
    #include "wx/app.h"
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/cmdline.h"
#include "wx/dynlib.h"
#include "wx/scopeguard.h"

#include "wx/msw/private.h"
#include "wx/msw/seh.h"

#if wxUSE_ON_FATAL_EXCEPTION
    #include "wx/datetime.h"
    #include "wx/msw/crashrpt.h"
#endif // wxUSE_ON_FATAL_EXCEPTION

#ifdef __WXWINCE__
    // there is no ExitProcess() under CE but exiting the main thread has the
    // same effect
    #ifndef ExitProcess
        #define ExitProcess ExitThread
    #endif
#endif // __WXWINCE__

#ifdef __BORLANDC__
    // BC++ has to be special: its run-time expects the DLL entry point to be
    // named DllEntryPoint instead of the (more) standard DllMain
    #define DllMain DllEntryPoint
#endif // __BORLANDC__

#if defined(__WXMICROWIN__)
    #define HINSTANCE HANDLE
#endif

// defined in common/init.cpp
extern int wxEntryReal(int& argc, wxChar **argv);
extern int wxEntryCleanupReal(int& argc, wxChar **argv);

// ============================================================================
// implementation: various entry points
// ============================================================================

#if wxUSE_BASE

// ----------------------------------------------------------------------------
// wrapper wxEntry catching all Win32 exceptions occurring in a wx program
// ----------------------------------------------------------------------------

// wrap real wxEntry in a try-except block to be able to call
// OnFatalException() if necessary
#if wxUSE_ON_FATAL_EXCEPTION

// global pointer to exception information, only valid inside OnFatalException,
// used by wxStackWalker and wxCrashReport
extern EXCEPTION_POINTERS *wxGlobalSEInformation = NULL;

// flag telling us whether the application wants to handle exceptions at all
static bool gs_handleExceptions = false;

static void wxFatalExit()
{
    // use the same exit code as abort()
    ::ExitProcess(3);
}

unsigned long wxGlobalSEHandler(EXCEPTION_POINTERS *pExcPtrs)
{
    if ( gs_handleExceptions && wxTheApp )
    {
        // store the pointer to exception info
        wxGlobalSEInformation = pExcPtrs;

        // give the user a chance to do something special about this
        wxSEH_TRY
        {
            wxTheApp->OnFatalException();
        }
        wxSEH_IGNORE      // ignore any exceptions inside the exception handler

        wxGlobalSEInformation = NULL;

        // this will execute our handler and terminate the process
        return EXCEPTION_EXECUTE_HANDLER;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

#ifdef __VISUALC__

void wxSETranslator(unsigned int WXUNUSED(code), EXCEPTION_POINTERS *ep)
{
    switch ( wxGlobalSEHandler(ep) )
    {
        default:
            wxFAIL_MSG( wxT("unexpected wxGlobalSEHandler() return value") );
            // fall through

        case EXCEPTION_EXECUTE_HANDLER:
            // if wxApp::OnFatalException() had been called we should exit the
            // application -- but we shouldn't kill our host when we're a DLL
#ifndef WXMAKINGDLL
            wxFatalExit();
#endif // not a DLL
            break;

        case EXCEPTION_CONTINUE_SEARCH:
            // we're called for each "catch ( ... )" and if we (re)throw from
            // here, the catch handler body is not executed, so the effect is
            // as if had inhibited translation of SE to C++ ones because the
            // handler will never see any structured exceptions
            throw;
    }
}

#endif // __VISUALC__

bool wxHandleFatalExceptions(bool doit)
{
    // assume this can only be called from the main thread
    gs_handleExceptions = doit;

    return true;
}

int wxEntry(int& argc, wxChar **argv)
{
    DisableAutomaticSETranslator();

    wxSEH_TRY
    {
        return wxEntryReal(argc, argv);
    }
    wxSEH_HANDLE(-1)
}

#else // !wxUSE_ON_FATAL_EXCEPTION

int wxEntry(int& argc, wxChar **argv)
{
    return wxEntryReal(argc, argv);
}

#endif // wxUSE_ON_FATAL_EXCEPTION/!wxUSE_ON_FATAL_EXCEPTION

#endif // wxUSE_BASE

// ----------------------------------------------------------------------------
// global HINSTANCE
// ----------------------------------------------------------------------------

#if wxUSE_BASE

HINSTANCE wxhInstance = 0;

extern "C" HINSTANCE wxGetInstance()
{
    return wxhInstance;
}

void wxSetInstance(HINSTANCE hInst)
{
    wxhInstance = hInst;
}

#endif // wxUSE_BASE
