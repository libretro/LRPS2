/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Threading.h"
#include "EventSource.h"

namespace Threading
{

// --------------------------------------------------------------------------------------
//  ThreadDeleteEvent
// --------------------------------------------------------------------------------------
class EventListener_Thread : public IEventDispatcher<int>
{
public:
    typedef int EvtParams;
    EventListener_Thread()
    {
    }

    virtual ~EventListener_Thread() = default;

    void DispatchEvent(const int &params)
    {
        OnThreadCleanup();
    }

protected:
    // Invoked by the pxThread when the thread execution is ending.  This is
    // typically more useful than a delete listener since the extended thread information
    // provided by virtualized functions/methods will be available.
    // Important!  This event is executed *by the thread*, so care must be taken to ensure
    // thread sync when necessary (posting messages to the main thread, etc).
    virtual void OnThreadCleanup() = 0;
};

// --------------------------------------------------------------------------------------
// pxThread - Helper class for the basics of starting/managing persistent threads.
// --------------------------------------------------------------------------------------
// This class is meant to be a helper for the typical threading model of "start once and
// reuse many times."  This class incorporates a lot of extra overhead in stopping and
// starting threads, but in turn provides most of the basic thread-safety and event-handling
// functionality needed for a threaded operation.  In practice this model is usually an
// ideal one for efficiency since Operating Systems themselves typically subscribe to a
// design where sleeping, suspending, and resuming threads is very efficient, but starting
// new threads has quite a bit of overhead.
//
// To use this as a base class for your threaded procedure, overload the following virtual
// methods:
//  void OnStart();
//  void ExecuteTaskInThread();
//  void OnCleanupInThread();
//
// Use the public methods Start() and Cancel() to start and shutdown the thread, and use
// m_sem_event internally to post/receive events for the thread (make a public accessor for
// it in your derived class if your thread utilizes the post).
//
// Notes:
//  * Constructing threads as static global vars isn't recommended since it can potentially
//    confuse w32pthreads, if the static initializers are executed out-of-order (C++ offers
//    no dependency options for ensuring correct static var initializations).  Use heap
//    allocation to create thread objects instead.
//
class pxThread
{
    DeclareNoncopyableObject(pxThread);
protected:
    wxString m_name; // diagnostic name for our thread.
    pthread_t m_thread;

    Semaphore m_sem_event;      // general wait event that's needed by most threads
    Semaphore m_sem_startup;    // startup sync tool
    Mutex m_mtx_InThread;       // used for canceling and closing threads in a deadlock-safe manner
    MutexRecursive m_mtx_start; // used to lock the Start() code from starting simultaneous threads accidentally.

    std::atomic<bool> m_detached; // a boolean value which indicates if the m_thread handle is valid
    std::atomic<bool> m_running;  // set true by Start(), and set false by Cancel(), Block(), etc.

    EventSource<EventListener_Thread> m_evtsrc_OnDelete;


public:
    virtual ~pxThread();
    pxThread(const wxString &name = L"pxThread");

    virtual void Start();
    virtual void Cancel();
    virtual bool Cancel(const wxTimeSpan &timeout);

    bool WaitOnSelf(Mutex &mutex, const wxTimeSpan &timeout) const;

    bool IsRunning() const;
    bool IsSelf() const;
    void _ThreadCleanup();
    void _internal_execute();

protected:
    // Extending classes should always implement your own OnStart(), which is called by
    // Start() once necessary locks have been obtained.  Do not override Start() directly
    // unless you're really sure that's what you need to do. ;)
    virtual void OnStart();

    virtual void OnStartInThread();

    // This is called when the thread has been canceled or exits normally.  The pxThread
    // automatically binds it to the pthread cleanup routines as soon as the thread starts.
    virtual void OnCleanupInThread();

    // Implemented by derived class to perform actual threaded task!
    virtual void ExecuteTaskInThread() = 0;

    void TestCancel() const;

    // ----------------------------------------------------------------------------
    // Section of methods for internal use only.
    void _try_virtual_invoke(void (pxThread::*method)());
};
}
