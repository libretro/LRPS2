///////////////////////////////////////////////////////////////////////////////
// Name:        wx/evtloop.h
// Purpose:     declares wxEventLoop class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     01.06.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EVTLOOP_H_
#define _WX_EVTLOOP_H_

#include "wx/event.h"
#include "wx/utils.h"

// TODO: implement wxEventLoopSource for MSW (it should wrap a HANDLE and be
//       monitored using MsgWaitForMultipleObjects())
#if defined(__WXOSX__) || (defined(__UNIX__) && !defined(__WINDOWS__))
    #define wxUSE_EVENTLOOP_SOURCE 1
#else
    #define wxUSE_EVENTLOOP_SOURCE 0
#endif

#if wxUSE_EVENTLOOP_SOURCE
    class wxEventLoopSource;
    class wxEventLoopSourceHandler;
#endif

// ----------------------------------------------------------------------------
// wxEventLoopBase: interface for wxEventLoop
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxEventLoopBase
{
public:
    // trivial, but needed (because of wxEventLoopBase) ctor
    wxEventLoopBase();

    // dtor
    virtual ~wxEventLoopBase() { }

    // use this to check whether the event loop was successfully created before
    // using it
    virtual bool IsOk() const { return true; }

    // returns true if this is the main loop
    bool IsMain() const;

    #if wxUSE_EVENTLOOP_SOURCE
        // create a new event loop source wrapping the given file descriptor and
        // monitor it for events occurring on this descriptor in all event loops
        static wxEventLoopSource *
          AddSourceForFD(int fd, wxEventLoopSourceHandler *handler, int flags);
    #endif // wxUSE_EVENTLOOP_SOURCE

    // dispatch&processing
    // -------------------

    // start the event loop, return the exit code when it is finished
    //
    // notice that wx ports should override DoRun(), this method is virtual
    // only to allow overriding it in the user code for custom event loops
    virtual int Run();

    // is this event loop running now?
    //
    // notice that even if this event loop hasn't terminated yet but has just
    // spawned a nested (e.g. modal) event loop, this would return false
    bool IsRunning() const;

    // exit from the loop with the given exit code
    //
    // this can be only used to exit the currently running loop, use
    // ScheduleExit() if this might not be the case
    virtual void Exit(int rc = 0);

    // ask the event loop to exit with the given exit code, can be used even if
    // this loop is not running right now but the loop must have been started,
    // i.e. Run() should have been already called
    virtual void ScheduleExit(int rc = 0) = 0;

    // return true if any events are available
    virtual bool Pending() const = 0;

    // dispatch a single event, return false if we should exit from the loop
    virtual bool Dispatch() = 0;

    // implement this to wake up the loop: usually done by posting a dummy event
    // to it (can be called from non main thread)
    virtual void WakeUp() = 0;


    // idle handling
    // -------------

        // make sure that idle events are sent again
    virtual void WakeUpIdle();

        // this virtual function is called  when the application
        // becomes idle and by default it forwards to wxApp::ProcessIdle() and
        // while it can be overridden in a custom event loop, you must call the
        // base class version to ensure that idle events are still generated
        //
        // it should return true if more idle events are needed, false if not
    virtual bool ProcessIdle();


    // active loop
    // -----------

    // return currently active (running) event loop, may be NULL
    static wxEventLoopBase *GetActive() { return ms_activeLoop; }

    // set currently active (running) event loop
    static void SetActive(wxEventLoopBase* loop);


protected:
    // real implementation of Run()
    virtual int DoRun() = 0;

    // this function should be called before the event loop terminates, whether
    // this happens normally (because of Exit() call) or abnormally (because of
    // an exception thrown from inside the loop)
    virtual void OnExit();

    // Return true if we're currently inside our Run(), even if another nested
    // event loop is currently running, unlike IsRunning() (which should have
    // been really called IsActive() but it's too late to change this now).
    bool IsInsideRun() const { return m_isInsideRun; }

    // the pointer to currently active loop
    static wxEventLoopBase *ms_activeLoop;

    // should we exit the loop?
    bool m_shouldExit;

private:
    // this flag is set on entry into Run() and reset before leaving it
    bool m_isInsideRun;

    wxDECLARE_NO_COPY_CLASS(wxEventLoopBase);
};

#if defined(__WINDOWS__) || defined(__WXMAC__) || (defined(__UNIX__) && !defined(__WXOSX__))

// this class can be used to implement a standard event loop logic using
// Pending() and Dispatch()
//
// it also handles idle processing automatically
class WXDLLIMPEXP_BASE wxEventLoopManual : public wxEventLoopBase
{
public:
    wxEventLoopManual();

    // sets the "should exit" flag and wakes up the loop so that it terminates
    // soon
    virtual void ScheduleExit(int rc = 0);

protected:
    // enters a loop calling OnNextIteration(), Pending() and Dispatch() and
    // terminating when Exit() is called
    virtual int DoRun();

    // may be overridden to perform some action at the start of each new event
    // loop iteration
    virtual void OnNextIteration() { }


    // the loop exit code
    int m_exitcode;

private:
    // process all already pending events and dispatch a new one (blocking
    // until it appears in the event queue if necessary)
    //
    // returns the return value of Dispatch()
    bool ProcessEvents();

    wxDECLARE_NO_COPY_CLASS(wxEventLoopManual);
};

#endif // platforms using "manual" loop

// we're moving away from old m_impl wxEventLoop model as otherwise the user
// code doesn't have access to platform-specific wxEventLoop methods and this
// can sometimes be very useful (e.g. under MSW this is necessary for
// integration with MFC) but currently this is not done for all ports yet (e.g.
// wxX11) so fall back to the old wxGUIEventLoop definition below for them

#if defined(__DARWIN__)
    // CoreFoundation-based event loop is currently in wxBase so include it in
    // any case too (although maybe it actually shouldn't be there at all)
    #include "wx/osx/core/evtloop.h"
#endif

// include the header defining wxConsoleEventLoop
#if defined(__UNIX__) && !defined(__WINDOWS__)
    #include "wx/unix/evtloop.h"
#elif defined(__WINDOWS__)
    #include "wx/msw/evtloopconsole.h"
#endif

    // we can't define wxEventLoop differently in GUI and base libraries so use
    // a #define to still allow writing wxEventLoop in the user code
    #if wxUSE_CONSOLE_EVENTLOOP && (defined(__WINDOWS__) || defined(__UNIX__))
        #define wxEventLoop wxConsoleEventLoop
    #else // we still must define it somehow for the code below...
        #define wxEventLoop wxEventLoopBase
    #endif

inline bool wxEventLoopBase::IsRunning() const { return GetActive() == this; }

// ----------------------------------------------------------------------------
// wxEventLoopActivator: helper class for wxEventLoop implementations
// ----------------------------------------------------------------------------

// this object sets the wxEventLoop given to the ctor as the currently active
// one and unsets it in its dtor, this is especially useful in presence of
// exceptions but is more tidy even when we don't use them
class wxEventLoopActivator
{
public:
    wxEventLoopActivator(wxEventLoopBase *evtLoop)
    {
        m_evtLoopOld = wxEventLoopBase::GetActive();
        wxEventLoopBase::SetActive(evtLoop);
    }

    ~wxEventLoopActivator()
    {
        // restore the previously active event loop
        wxEventLoopBase::SetActive(m_evtLoopOld);
    }

private:
    wxEventLoopBase *m_evtLoopOld;
};


#endif // _WX_EVTLOOP_H_
