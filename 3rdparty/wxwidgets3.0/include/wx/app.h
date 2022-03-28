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
#include "wx/init.h"        // we must declare wxEntry()

class WXDLLIMPEXP_FWD_BASE wxAppConsole;
class WXDLLIMPEXP_FWD_BASE wxAppTraits;
class WXDLLIMPEXP_FWD_BASE wxEventLoopBase;

// ----------------------------------------------------------------------------
// typedefs
// ----------------------------------------------------------------------------

// the type of the function used to create a wxApp object on program start up
typedef wxAppConsole* (*wxAppInitializerFunction)();

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

    // This is only called if OnInit() returned true so it's a good place to do
    // any cleanup matching the initializations done there.
    virtual int OnExit();

    // This is the very last function called on wxApp object before it is
    // destroyed. If you override it (instead of overriding OnExit() as usual)
    // do not forget to call the base class version!
    virtual void CleanUp();

    // Called from wxExit() function, should terminate the application a.s.a.p.
    virtual void Exit();

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

// this macro can be used multiple times and just allows you to use wxGetApp()
// function
#define wxDECLARE_APP(appname)              \
    extern appname& wxGetApp()

#endif // _WX_APP_H_BASE_
