/////////////////////////////////////////////////////////////////////////////
// Name:        wx/app.h
// Purpose:     wxAppBase class and macros used for declaration of wxApp
//              derived class in the user code
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_APP_H_BASE_
#define _WX_APP_H_BASE_

// ----------------------------------------------------------------------------
// headers we have to include here
// ----------------------------------------------------------------------------

#include "wx/event.h"       // for the base class
#include "wx/eventfilter.h" // (and another one)
#include "wx/build.h"
#include "wx/cmdargs.h"     // for wxCmdLineArgsArray used by wxApp::argv
#include "wx/init.h"        // we must declare wxEntry()

class WXDLLIMPEXP_FWD_BASE wxAppConsole;
class WXDLLIMPEXP_FWD_BASE wxAppTraits;
class WXDLLIMPEXP_FWD_BASE wxCmdLineParser;
class WXDLLIMPEXP_FWD_BASE wxEventLoopBase;
class WXDLLIMPEXP_FWD_BASE wxMessageOutput;

// ----------------------------------------------------------------------------
// typedefs
// ----------------------------------------------------------------------------

// the type of the function used to create a wxApp object on program start up
typedef wxAppConsole* (*wxAppInitializerFunction)();

// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

// use of this list is strongly deprecated, use wxApp ScheduleForDestruction()
// and IsScheduledForDestruction()  methods instead of this list directly, it
// is here for compatibility purposes only
extern WXDLLIMPEXP_DATA_BASE(wxList) wxPendingDelete;

// ----------------------------------------------------------------------------
// wxAppConsoleBase: wxApp for non-GUI applications
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxAppConsoleBase : public wxEvtHandler,
                                          public wxEventFilter
{
public:
    // ctor and dtor
    wxAppConsoleBase();
    virtual ~wxAppConsoleBase();


    // the virtual functions which may/must be overridden in the derived class
    // -----------------------------------------------------------------------

    // This is the very first function called for a newly created wxApp object,
    // it is used by the library to do the global initialization. If, for some
    // reason, you must override it (instead of just overriding OnInit(), as
    // usual, for app-specific initializations), do not forget to call the base
    // class version!
    virtual bool Initialize(int& argc, wxChar **argv);

    // This gives wxCocoa a chance to call OnInit() with a memory pool in place
    virtual bool CallOnInit() { return OnInit(); }

    // Called before OnRun(), this is a good place to do initialization -- if
    // anything fails, return false from here to prevent the program from
    // continuing. The command line is normally parsed here, call the base
    // class OnInit() to do it.
    virtual bool OnInit();

    // This is the replacement for the normal main(): all program work should
    // be done here. When OnRun() returns, the programs starts shutting down.
    virtual int OnRun();

    // Called before the first events are handled, called from within MainLoop()
    virtual void OnLaunched();
    
    // This is called by wxEventLoopBase::SetActive(): you should put the code
    // which needs an active event loop here.
    // Note that this function is called whenever an event loop is activated;
    // you may want to use wxEventLoopBase::IsMain() to perform initialization
    // specific for the app's main event loop.
    virtual void OnEventLoopEnter(wxEventLoopBase* WXUNUSED(loop)) {}

    // This is only called if OnInit() returned true so it's a good place to do
    // any cleanup matching the initializations done there.
    virtual int OnExit();

    // This is called by wxEventLoopBase::OnExit() for each event loop which
    // is exited.
    virtual void OnEventLoopExit(wxEventLoopBase* WXUNUSED(loop)) {}

    // This is the very last function called on wxApp object before it is
    // destroyed. If you override it (instead of overriding OnExit() as usual)
    // do not forget to call the base class version!
    virtual void CleanUp();

    // Called when a fatal exception occurs, this function should take care not
    // to do anything which might provoke a nested exception! It may be
    // overridden if you wish to react somehow in non-default way (core dump
    // under Unix, application crash under Windows) to fatal program errors,
    // however extreme care should be taken if you don't want this function to
    // crash.
    virtual void OnFatalException() { }

    // Called from wxExit() function, should terminate the application a.s.a.p.
    virtual void Exit();

    // set/get the app class name
    wxString GetClassName() const { return m_className; }
    void SetClassName(const wxString& name) { m_className = name; }

    // miscellaneous customization functions
    // -------------------------------------

    // create the app traits object to which we delegate for everything which
    // either should be configurable by the user (then he can change the
    // default behaviour simply by overriding CreateTraits() and returning his
    // own traits object) or which is GUI/console dependent as then wxAppTraits
    // allows us to abstract the differences behind the common facade
    wxAppTraits *GetTraits();

    // this function provides safer access to traits object than
    // wxTheApp->GetTraits() during startup or termination when the global
    // application object itself may be unavailable
    //
    // of course, it still returns NULL in this case and the caller must check
    // for it
    static wxAppTraits *GetTraitsIfExists();

    // Return some valid traits object.
    //
    // This method checks if we have wxTheApp and returns its traits if it does
    // exist and the traits are non-NULL, similarly to GetTraitsIfExists(), but
    // falls back to wxConsoleAppTraits to ensure that it always returns
    // something valid.
    static wxAppTraits& GetValidTraits();

    // returns the main event loop instance, i.e. the event loop which is started
    // by OnRun() and which dispatches all events sent from the native toolkit
    // to the application (except when new event loops are temporarily set-up).
    // The returned value maybe NULL. Put initialization code which needs a
    // non-NULL main event loop into OnEventLoopEnter().
    wxEventLoopBase* GetMainLoop() const
        { return m_mainLoop; }

    // pending events
    // --------------

    // IMPORTANT: all these methods conceptually belong to wxEventLoopBase
    //            but for many reasons we need to allow queuing of events
    //            even when there's no event loop (e.g. in wxApp::OnInit);
    //            this feature is used e.g. to queue events on secondary threads
    //            or in wxPython to use wx.CallAfter before the GUI is initialized

    // process all events in the m_handlersWithPendingEvents list -- it is necessary
    // to call this function to process posted events. This happens during each
    // event loop iteration in GUI mode but if there is no main loop, it may be
    // also called directly.
    virtual void ProcessPendingEvents();

    // check if there are pending events on global pending event list
    bool HasPendingEvents() const;

    // called by ~wxEvtHandler to (eventually) remove the handler from the list of
    // the handlers with pending events
    void RemovePendingEventHandler(wxEvtHandler* toRemove);

    // adds an event handler to the list of the handlers with pending events
    void AppendPendingEventHandler(wxEvtHandler* toAppend);

    // moves the event handler from the list of the handlers with pending events
    //to the list of the handlers with _delayed_ pending events
    void DelayPendingEventHandler(wxEvtHandler* toDelay);

    // deletes the current pending events
    void DeletePendingEvents();

    // wxEventLoop-related methods
    // ---------------------------

    // all these functions are forwarded to the corresponding methods of the
    // currently active event loop -- and do nothing if there is none
    virtual bool Pending();
    virtual bool Dispatch();

    virtual int MainLoop();
    virtual void ExitMainLoop();

    virtual void WakeUpIdle();

    // this method is called by the active event loop when there are no events
    // to process
    //
    // by default it generates the idle events and if you override it in your
    // derived class you should call the base class version to ensure that idle
    // events are still sent out
    virtual bool ProcessIdle();

    // debugging support
    // -----------------

    // this function is called when an assert failure occurs, the base class
    // version does the normal processing (i.e. shows the usual assert failure
    // dialog box)
    //
    // the arguments are the location of the failed assert (func may be empty
    // if the compiler doesn't support C99 __FUNCTION__), the text of the
    // assert itself and the user-specified message
    virtual void OnAssertFailure(const wxChar *file,
                                 int line,
                                 const wxChar *func,
                                 const wxChar *cond,
                                 const wxChar *msg);

    // old version of the function without func parameter, for compatibility
    // only, override OnAssertFailure() in the new code
    virtual void OnAssert(const wxChar *file,
                          int line,
                          const wxChar *cond,
                          const wxChar *msg);

    // implementation only from now on
    // -------------------------------

    // helpers for dynamic wxApp construction
    static void SetInitializerFunction(wxAppInitializerFunction fn)
        { ms_appInitFn = fn; }
    static wxAppInitializerFunction GetInitializerFunction()
        { return ms_appInitFn; }

    // accessors for ms_appInstance field (external code might wish to modify
    // it, this is why we provide a setter here as well, but you should really
    // know what you're doing if you call it), wxTheApp is usually used instead
    // of GetInstance()
    static wxAppConsole *GetInstance() { return ms_appInstance; }
    static void SetInstance(wxAppConsole *app) { ms_appInstance = app; }


    // command line arguments (public for backwards compatibility)
    int argc;

    // this object is implicitly convertible to either "char**" (traditional
    // type of argv parameter of main()) or to "wchar_t **" (for compatibility
    // with Unicode build in previous wx versions and because the command line
    // can, in pr
#if wxUSE_UNICODE
    wxCmdLineArgsArray argv;
#else
    char **argv;
#endif

protected:
    // delete all objects in wxPendingDelete list
    //
    // called from ProcessPendingEvents()
    void DeletePendingObjects();

    // the function which creates the traits object when GetTraits() needs it
    // for the first time
    virtual wxAppTraits *CreateTraits();

    // function used for dynamic wxApp creation
    static wxAppInitializerFunction ms_appInitFn;

    // the one and only global application object
    static wxAppConsole *ms_appInstance;

    // create main loop from AppTraits or return NULL if
    // there is no main loop implementation
    wxEventLoopBase *CreateMainLoop();

    // application info (must be set from the user code)
    wxString 
             m_appName,           // app name ("myapp")
             m_appDisplayName,    // app display name ("My Application")
             m_className;         // class name

    // the class defining the application behaviour, NULL initially and created
    // by GetTraits() when first needed
    wxAppTraits *m_traits;

    // the main event loop of the application (may be NULL if the loop hasn't
    // been started yet or has already terminated)
    wxEventLoopBase *m_mainLoop;


    // pending events management vars:

    // the array of the handlers with pending events which needs to be processed
    // inside ProcessPendingEvents()
    wxEvtHandlerArray m_handlersWithPendingEvents;

    // helper array used by ProcessPendingEvents() to store the event handlers
    // which have pending events but of these events none can be processed right now
    // (because of a call to wxEventLoop::YieldFor() which asked to selectively process
    // pending events)
    wxEvtHandlerArray m_handlersWithPendingDelayedEvents;

#if wxUSE_THREADS
    // this critical section protects both the lists above
    wxCriticalSection m_handlersWithPendingEventsLocker;
#endif

    // flag modified by Suspend/ResumeProcessingOfPendingEvents()
    bool m_bDoPendingEventProcessing;

    friend class WXDLLIMPEXP_FWD_BASE wxEvtHandler;

    // the application object is a singleton anyhow, there is no sense in
    // copying it
    wxDECLARE_NO_COPY_CLASS(wxAppConsoleBase);
};

#if defined(__UNIX__) && !defined(__WINDOWS__)
    #include "wx/unix/app.h"
#else
    // this has to be a class and not a typedef as we forward declare it
    class wxAppConsole : public wxAppConsoleBase { };
#endif

// ----------------------------------------------------------------------------
// wxAppBase: the common part of wxApp implementations for all platforms
// ----------------------------------------------------------------------------


// wxApp is defined in core and we cannot define another one in wxBase,
// so use the preprocessor to allow using wxApp in console programs too
#define wxApp wxAppConsole

// ----------------------------------------------------------------------------
// the global data
// ----------------------------------------------------------------------------

// for compatibility, we define this macro to access the global application
// object of type wxApp
//
// note that instead of using of wxTheApp in application code you should
// consider using wxDECLARE_APP() after which you may call wxGetApp() which will
// return the object of the correct type (i.e. MyApp and not wxApp)
//
// the cast is safe as in GUI build we only use wxApp, not wxAppConsole, and in
// console mode it does nothing at all
#define wxTheApp static_cast<wxApp*>(wxApp::GetInstance())

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

// event loop related functions only work in GUI programs
// ------------------------------------------------------

// Force an exit from main loop
WXDLLIMPEXP_BASE void wxExit();

// Yield to other apps/messages
WXDLLIMPEXP_BASE void wxWakeUpIdle();

// ----------------------------------------------------------------------------
// macros for dynamic creation of the application object
// ----------------------------------------------------------------------------

// Having a global instance of this class allows wxApp to be aware of the app
// creator function. wxApp can then call this function to create a new app
// object. Convoluted, but necessary.

class WXDLLIMPEXP_BASE wxAppInitializer
{
public:
    wxAppInitializer(wxAppInitializerFunction fn)
        { wxApp::SetInitializerFunction(fn); }
};

// the code below defines a wxIMPLEMENT_WXWIN_MAIN macro which you can use if
// your compiler really, really wants main() to be in your main program (e.g.
// hello.cpp). Now wxIMPLEMENT_APP should add this code if required.

// For compilers that support it, prefer to use wmain() as this ensures any
// Unicode strings can be passed as command line parameters and not just those
// representable in the current locale.
#if wxUSE_UNICODE && defined(__VISUALC__)
    #define wxIMPLEMENT_WXWIN_MAIN_CONSOLE                                    \
        int wmain(int argc, wchar_t **argv)                                   \
        {                                                                     \
            return wxEntry(argc, argv);                                       \
        }
#else // Use standard main()
    #define wxIMPLEMENT_WXWIN_MAIN_CONSOLE                                    \
        int main(int argc, char **argv)                                       \
        {                                                                     \
            return wxEntry(argc, argv);                                       \
        }
#endif

// port-specific header could have defined it already in some special way
#ifndef wxIMPLEMENT_WXWIN_MAIN
    #define wxIMPLEMENT_WXWIN_MAIN          wxIMPLEMENT_WXWIN_MAIN_CONSOLE
#endif // defined(wxIMPLEMENT_WXWIN_MAIN)

#ifdef __WXUNIVERSAL__
    #include "wx/univ/theme.h"

    #ifdef wxUNIV_DEFAULT_THEME
        #define wxIMPLEMENT_WX_THEME_SUPPORT \
            WX_USE_THEME(wxUNIV_DEFAULT_THEME);
    #else
        #define wxIMPLEMENT_WX_THEME_SUPPORT
    #endif
#else
    #define wxIMPLEMENT_WX_THEME_SUPPORT
#endif

// Use this macro if you want to define your own main() or WinMain() function
// and call wxEntry() from there.
#define wxIMPLEMENT_APP_NO_MAIN(appname)                                    \
    appname& wxGetApp() { return *static_cast<appname*>(wxApp::GetInstance()); }    \
    wxAppConsole *wxCreateApp()                                             \
    {                                                                       \
        return new appname;                                                 \
    }                                                                       \
    wxAppInitializer                                                        \
        wxTheAppInitializer((wxAppInitializerFunction) wxCreateApp)

// Same as wxIMPLEMENT_APP() normally but doesn't include themes support in
// wxUniversal builds
#define wxIMPLEMENT_APP_NO_THEMES(appname)  \
    wxIMPLEMENT_WXWIN_MAIN                  \
    wxIMPLEMENT_APP_NO_MAIN(appname)

// Use this macro exactly once, the argument is the name of the wxApp-derived
// class which is the class of your application.
#define wxIMPLEMENT_APP(appname)            \
    wxIMPLEMENT_WX_THEME_SUPPORT            \
    wxIMPLEMENT_APP_NO_THEMES(appname)

// Same as IMPLEMENT_APP(), but for console applications.
#define wxIMPLEMENT_APP_CONSOLE(appname)    \
    wxIMPLEMENT_WXWIN_MAIN_CONSOLE          \
    wxIMPLEMENT_APP_NO_MAIN(appname)

// this macro can be used multiple times and just allows you to use wxGetApp()
// function
#define wxDECLARE_APP(appname)              \
    extern appname& wxGetApp()


// declare the stuff defined by wxIMPLEMENT_APP() macro, it's not really needed
// anywhere else but at the very least it suppresses icc warnings about
// defining extern symbols without prior declaration, and it shouldn't do any
// harm
extern wxAppConsole *wxCreateApp();
extern wxAppInitializer wxTheAppInitializer;

// ----------------------------------------------------------------------------
// Compatibility macro aliases
// ----------------------------------------------------------------------------

// deprecated variants _not_ requiring a semicolon after them
// (note that also some wx-prefixed macro do _not_ require a semicolon because
//  it's not always possible to force the compire to require it)

#define IMPLEMENT_WXWIN_MAIN_CONSOLE            wxIMPLEMENT_WXWIN_MAIN_CONSOLE
#define IMPLEMENT_WXWIN_MAIN                    wxIMPLEMENT_WXWIN_MAIN
#define IMPLEMENT_WX_THEME_SUPPORT              wxIMPLEMENT_WX_THEME_SUPPORT
#define IMPLEMENT_APP_NO_MAIN(app)              wxIMPLEMENT_APP_NO_MAIN(app);
#define IMPLEMENT_APP_NO_THEMES(app)            wxIMPLEMENT_APP_NO_THEMES(app);
#define IMPLEMENT_APP(app)                      wxIMPLEMENT_APP(app);
#define IMPLEMENT_APP_CONSOLE(app)              wxIMPLEMENT_APP_CONSOLE(app);
#define DECLARE_APP(app)                        wxDECLARE_APP(app);

#endif // _WX_APP_H_BASE_
