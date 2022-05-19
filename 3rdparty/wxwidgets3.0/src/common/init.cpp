/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/init.cpp
// Purpose:     initialisation for the library
// Author:      Vadim Zeitlin
// Modified by:
// Created:     04.10.99
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/filefn.h"
    #include "wx/module.h"
#endif

#include "wx/init.h"
#include "wx/thread.h"

#include "wx/scopedptr.h"

#if defined(__WINDOWS__)
    #include "wx/msw/private.h"
#endif

#if wxUSE_UNICODE && defined(__WXOSX__)
    #include <locale.h>
#endif

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// we need a dummy app object if the user doesn't want to create a real one
class wxDummyConsoleApp : public wxAppConsole
{
public:
    wxDummyConsoleApp() { }

    virtual int OnRun() { return 0; }

    wxDECLARE_NO_COPY_CLASS(wxDummyConsoleApp);
};

// we need a special kind of auto pointer to wxApp which not only deletes the
// pointer it holds in its dtor but also resets the global application pointer
wxDECLARE_SCOPED_PTR(wxAppConsole, wxAppPtrBase)
wxDEFINE_SCOPED_PTR(wxAppConsole, wxAppPtrBase)

class wxAppPtr : public wxAppPtrBase
{
public:
    wxEXPLICIT wxAppPtr(wxAppConsole *ptr = NULL) : wxAppPtrBase(ptr) { }
    ~wxAppPtr()
    {
        if ( get() )
        {
            // the pointer is going to be deleted in the base class dtor, don't
            // leave the dangling pointer!
            wxApp::SetInstance(NULL);
        }
    }

    void Set(wxAppConsole *ptr)
    {
        reset(ptr);

        wxApp::SetInstance(ptr);
    }

    wxDECLARE_NO_COPY_CLASS(wxAppPtr);
};

// class to ensure that wxAppBase::CleanUp() is called if our Initialize()
// fails
class wxCallAppCleanup
{
public:
    wxCallAppCleanup(wxAppConsole *app) : m_app(app) { }
    ~wxCallAppCleanup() { if ( m_app ) m_app->CleanUp(); }

    void Dismiss() { m_app = NULL; }

private:
    wxAppConsole *m_app;
};

// ----------------------------------------------------------------------------
// initialization data
// ----------------------------------------------------------------------------

static struct InitData
{
    InitData()
    {
        nInitCount = 0;

#if wxUSE_UNICODE
        argc = argcOrig = 0;
        // argv = argvOrig = NULL; -- not even really needed
#endif // wxUSE_UNICODE
    }

    // critical section protecting this struct
    wxCRIT_SECT_DECLARE_MEMBER(csInit);

    // number of times wxInitialize() was called minus the number of times
    // wxUninitialize() was
    size_t nInitCount;

#if wxUSE_UNICODE
    int argc;

    // if we receive the command line arguments as ASCII and have to convert
    // them to Unicode ourselves (this is the case under Unix but not Windows,
    // for example), we remember the converted argv here because we'll have to
    // free it when doing cleanup to avoid memory leaks
    wchar_t **argv;

    // we also need to keep two copies, one passed to other functions, and one
    // unmodified original; somebody may modify the former, so we need to have
    // the latter to be able to free everything correctly
    int argcOrig;
    wchar_t **argvOrig;
#endif // wxUSE_UNICODE

    wxDECLARE_NO_COPY_CLASS(InitData);
} gs_initData;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// command line arguments ANSI -> Unicode conversion
// ----------------------------------------------------------------------------

#if wxUSE_UNICODE

static void ConvertArgsToUnicode(int argc, char **argv)
{
    gs_initData.argvOrig = new wchar_t *[argc + 1];
    gs_initData.argv = new wchar_t *[argc + 1];

    int wargc = 0;
    for ( int i = 0; i < argc; i++ )
    {
#ifdef __DARWIN__
        wxWCharBuffer buf(wxConvFileName->cMB2WX(argv[i]));
#else
        wxWCharBuffer buf(wxConvLocal.cMB2WX(argv[i]));
#endif
        if ( !buf ) { }
        else // converted ok
        {
            gs_initData.argvOrig[wargc] = gs_initData.argv[wargc] = wxStrdup(buf);
            wargc++;
        }
    }

    gs_initData.argcOrig = gs_initData.argc = wargc;
    gs_initData.argvOrig[wargc] =gs_initData.argv[wargc] = NULL;
}

static void FreeConvertedArgs()
{
    if ( gs_initData.argvOrig )
    {
        for ( int i = 0; i < gs_initData.argcOrig; i++ )
        {
            free(gs_initData.argvOrig[i]);
            // gs_initData.argv[i] normally points to the same data
        }

        wxDELETEA(gs_initData.argvOrig);
        wxDELETEA(gs_initData.argv);
        gs_initData.argcOrig = gs_initData.argc = 0;
    }
}

#endif // wxUSE_UNICODE

// ----------------------------------------------------------------------------
// start up
// ----------------------------------------------------------------------------

// initialization which is always done (not customizable) before wxApp creation
static bool DoCommonPreInit()
{
#if wxUSE_UNICODE && defined(__WXOSX__)
    // In OS X and iOS, wchar_t CRT functions convert to char* and fail under
    // some locales. The safest fix is to set LC_CTYPE to UTF-8 to ensure that
    // they can handle any input.
    //
    // Note that this must be done for any app, Cocoa or console, whether or
    // not it uses wxLocale.
    //
    // See http://stackoverflow.com/questions/11713745/why-does-the-printf-family-of-functions-care-about-locale
    setlocale(LC_CTYPE, "UTF-8");
#endif // wxUSE_UNICODE && defined(__WXOSX__)

#ifdef __WINDOWS__
    // GUI applications obtain HINSTANCE in their WinMain() but we also need to
    // initialize the global wxhInstance variable for the console programs as
    // they may need it too, so set it here if it wasn't done yet
    if ( !wxGetInstance() )
    {
        wxSetInstance(::GetModuleHandle(NULL));
    }
#endif // __WINDOWS__

    return true;
}

// non customizable initialization done after wxApp creation and initialization
static bool DoCommonPostInit()
{
    wxModule::RegisterModules();

    if ( !wxModule::InitializeModules() )
        return false;

    return true;
}

bool wxEntryStart(int& argc, wxChar **argv)
{
    // do minimal, always necessary, initialization
    // --------------------------------------------

    // initialize wxRTTI
    if ( !DoCommonPreInit() )
        return false;


    // first of all, we need an application object
    // -------------------------------------------

    // the user might have already created it himself somehow
    wxAppPtr app(wxTheApp);
    if ( !app.get() )
    {
        // if not, he might have used IMPLEMENT_APP() to give us a function to
        // create it
        wxAppInitializerFunction fnCreate = wxApp::GetInitializerFunction();

        if ( fnCreate )
        {
            // he did, try to create the custom wxApp object
            app.Set((*fnCreate)());
        }
    }

    if ( !app.get() )
    {
        // either IMPLEMENT_APP() was not used at all or it failed -- in any
        // case we still need something
        app.Set(new wxDummyConsoleApp);
    }


    // wxApp initialization: this can be customized
    // --------------------------------------------

    if ( !app->Initialize(argc, argv) )
        return false;

    // remember, possibly modified (e.g. due to removal of toolkit-specific
    // parameters), command line arguments in member variables
    app->argc = argc;

    wxCallAppCleanup callAppCleanup(app.get());


    // common initialization after wxTheApp creation
    // ---------------------------------------------

    if ( !DoCommonPostInit() )
        return false;


    // prevent the smart pointer from destroying its contents
    app.release();

    // and the cleanup object from doing cleanup
    callAppCleanup.Dismiss();

    return true;
}

#if wxUSE_UNICODE

// we provide a wxEntryStart() wrapper taking "char *" pointer too
bool wxEntryStart(int& argc, char **argv)
{
    ConvertArgsToUnicode(argc, argv);

    if ( !wxEntryStart(gs_initData.argc, gs_initData.argv) )
    {
        FreeConvertedArgs();

        return false;
    }

    return true;
}

#endif // wxUSE_UNICODE

// ----------------------------------------------------------------------------
// clean up
// ----------------------------------------------------------------------------

// cleanup done after destroying wxTheApp
static void DoCommonPostCleanup()
{
    wxModule::CleanUpModules();

    // we can't do this in wxApp itself because it doesn't know if argv had
    // been allocated
#if wxUSE_UNICODE
    FreeConvertedArgs();
#endif // wxUSE_UNICODE
}

void wxEntryCleanup()
{
    // delete the application object
    if ( wxTheApp )
    {
        wxTheApp->CleanUp();

        // reset the global pointer to it to NULL before destroying it as in
        // some circumstances this can result in executing the code using
        // wxTheApp and using half-destroyed object is no good
        wxAppConsole * const app = wxApp::GetInstance();
        wxApp::SetInstance(NULL);
        delete app;
    }


    DoCommonPostCleanup();
}

// ----------------------------------------------------------------------------
// wxEntry
// ----------------------------------------------------------------------------

// for MSW the real wxEntry is defined in msw/main.cpp
#ifndef __WINDOWS__
    #define wxEntryReal wxEntry
#endif // !__WINDOWS__

int wxEntryReal(int& argc, wxChar **argv)
{
    // library initialization
    wxInitializer initializer(argc, argv);

    if ( !initializer.IsOk() )
        return -1;

    // app initialization
    // don't call OnExit() if OnInit() failed
    if ( !wxTheApp->CallOnInit() )
	    return -1;

    // ensure that OnExit() is called if OnInit() had succeeded
    class CallOnExit
    {
	    public:
		    ~CallOnExit() { wxTheApp->OnExit(); }
    } callOnExit;

    // app execution
    return wxTheApp->OnRun();
}

#if wxUSE_UNICODE

// as with wxEntryStart, we provide an ANSI wrapper
int wxEntry(int& argc, char **argv)
{
    ConvertArgsToUnicode(argc, argv);

    return wxEntry(gs_initData.argc, gs_initData.argv);
}

#endif // wxUSE_UNICODE

// ----------------------------------------------------------------------------
// wxInitialize/wxUninitialize
// ----------------------------------------------------------------------------

bool wxInitialize()
{
    return wxInitialize(0, (wxChar**)NULL);
}

bool wxInitialize(int argc, wxChar **argv)
{
    wxCRIT_SECT_LOCKER(lockInit, gs_initData.csInit);

    if ( gs_initData.nInitCount++ )
    {
        // already initialized
        return true;
    }

    return wxEntryStart(argc, argv);
}

#if wxUSE_UNICODE
bool wxInitialize(int argc, char **argv)
{
    wxCRIT_SECT_LOCKER(lockInit, gs_initData.csInit);

    if ( gs_initData.nInitCount++ )
    {
        // already initialized
        return true;
    }

    return wxEntryStart(argc, argv);
}
#endif // wxUSE_UNICODE

void wxUninitialize()
{
    wxCRIT_SECT_LOCKER(lockInit, gs_initData.csInit);

    if ( --gs_initData.nInitCount == 0 )
    {
        wxEntryCleanup();
    }
}
