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

#ifdef __linux__
#include <signal.h> // for pthread_kill, which is in pthread.h on w32-pthreads
#include <sys/prctl.h>
#elif defined(__unix__)
#include <pthread_np.h>
#endif
#ifdef _WIN32
#include <direct.h>
#include <intrin.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <utility>

#include <wx/datetime.h>

#include "Dependencies.h"

#include "PersistentThread.h"
#include "EventSource.inl"

#include "../../libretro/retro_messager.h"

using namespace Threading;

template class EventSource<EventListener_Thread>;

class StaticMutex : public Mutex
{
protected:
    bool &m_DeletedFlag;

public:
    StaticMutex(bool &deletedFlag)
        : m_DeletedFlag(deletedFlag)
    {
    }

    virtual ~StaticMutex()
    {
        m_DeletedFlag = true;
    }
};

static pthread_key_t curthread_key = 0;
static s32 total_key_count = 0;

static bool tkl_destructed = false;
static StaticMutex total_key_lock(tkl_destructed);

static void make_curthread_key(const pxThread *thr)
{
    ScopedLock lock(total_key_lock);
    if (total_key_count++ != 0)
        return;

    if (pthread_key_create(&curthread_key, NULL) != 0)
        curthread_key = 0;
}

static void unmake_curthread_key(void)
{
    ScopedLock lock;
    if (!tkl_destructed)
        lock.AssignAndLock(total_key_lock);

    if (--total_key_count > 0)
        return;

    if (curthread_key)
        pthread_key_delete(curthread_key);

    curthread_key = 0;
}

Threading::pxThread::pxThread(const wxString &name)
    : m_name(name)
    , m_thread()
    , m_detached(true) // start out with m_thread in detached/invalid state
    , m_running(false)
{
}

// This destructor performs basic "last chance" cleanup, which is a blocking join
// against the thread. Extending classes should almost always implement their own
// thread closure process, since any pxThread will, by design, not terminate
// unless it has been properly canceled (resulting in deadlock).
//
// Thread safety: This class must not be deleted from its own thread.  That would be
// like marrying your sister, and then cheating on her with your daughter.
Threading::pxThread::~pxThread()
{
	if (m_running)
		m_mtx_InThread.Release();
	if (!m_detached.exchange(true))
		pthread_detach(m_thread);
}

static void _pt_callback_cleanup(void *handle)
{
    ((pxThread *)handle)->_ThreadCleanup();
}

// passed into pthread_create, and is used to dispatch the thread's object oriented
// callback function
static void *_internal_callback(void *itsme)
{
    if (itsme)
    {
	    pxThread &owner = *static_cast<pxThread *>(itsme);

	    pthread_cleanup_push(_pt_callback_cleanup, itsme);
	    owner._internal_execute();
	    pthread_cleanup_pop(true);
    }
    return NULL;
}

// Main entry point for starting or e-starting a persistent thread.  This function performs necessary
// locks and checks for avoiding race conditions, and then calls OnStart() immediately before
// the actual thread creation.  Extending classes should generally not override Start(), and should
// instead override DoPrepStart instead.
//
// This function should not be called from the owner thread.
void Threading::pxThread::Start()
{
    // Prevents sudden parallel startup, and or parallel startup + cancel:
    ScopedLock startlock(m_mtx_start);
    if (m_running)
        return;

    // clean up previous thread handle, if one exists.
    if (!m_detached.exchange(true))
	    pthread_detach(m_thread);
    OnStart();

    pthread_create(&m_thread, NULL, _internal_callback, this);
}

// Remarks:
//   Provision of non-blocking Cancel() is probably academic, since destroying a pxThread
//   object performs a blocking Cancel regardless of if you explicitly do a non-blocking Cancel()
//   prior, since the ExecuteTaskInThread() method requires a valid object state.  If you really need
//   fire-and-forget behavior on threads, use pthreads directly for now.
//
// This function should not be called from the owner thread.
//
// Exceptions raised by the blocking thread will be re-thrown into the main thread.  If isBlocking
// is false then no exceptions will occur.
//
void Threading::pxThread::Cancel()
{
    // Prevent simultaneous startup and cancel, necessary to avoid
    ScopedLock startlock(m_mtx_start);

    if (!m_running)
        return;

    if (m_detached)
        return;

    pthread_cancel(m_thread);

    if (!IsSelf())
    {
	    for (;;)
	    {
		    if (m_mtx_InThread.WaitWithoutYield(wxTimeSpan(0, 0, 0, 333)))
			    break;
	    }
    }
    if (!m_detached.exchange(true))
	    pthread_detach(m_thread);
}

bool Threading::pxThread::Cancel(const wxTimeSpan &timespan)
{
    // Prevent simultaneous startup and cancel:
    ScopedLock startlock(m_mtx_start);

    if (!m_running)
        return true;

    if (m_detached)
        return true;

    pthread_cancel(m_thread);

    if (!WaitOnSelf(m_mtx_InThread, timespan))
        return false;
    if (!m_detached.exchange(true))
	    pthread_detach(m_thread);
    return true;
}

bool Threading::pxThread::IsSelf() const
{
    // Detached threads may have their pthread handles recycled as newer threads, causing
    // false IsSelf reports.
    return !m_detached && (pthread_self() == m_thread);
}

bool Threading::pxThread::IsRunning() const
{
    return m_running;
}

// This helper function is a deadlock-safe method of waiting on a mutex in a pxThread.
//
// Note: Use of this function only applies to mutexes which are acquired by a worker thread.
// Calling this function from the context of the thread itself is an error, and a dev assertion
// will be generated.
//
// Exceptions:
//   This function will rethrow exceptions raised by the persistent thread, if it throws an
//   error while the calling thread is blocking (which also means the persistent thread has
//   terminated).
//
bool Threading::pxThread::WaitOnSelf(Mutex &mutex, const wxTimeSpan &timeout) const
{
    static const wxTimeSpan SelfWaitInterval(0, 0, 0, 333);
    if (IsSelf())
        return true;

    wxTimeSpan runningout(timeout);

    while (runningout.GetMilliseconds() > 0)
    {
        const wxTimeSpan interval((SelfWaitInterval < runningout) ? SelfWaitInterval : runningout);
        if (mutex.WaitWithoutYield(interval))
            return true;
        runningout -= interval;
    }
    return false;
}

// Inserts a thread cancellation point.  If the thread has received a cancel request, this
// function will throw an SEH exception designed to exit the thread (so make sure to use C++
// object encapsulation for anything that could leak resources, to ensure object unwinding
// and cleanup, or use the DoThreadCleanup() override to perform resource cleanup).
void Threading::pxThread::TestCancel() const
{
    pthread_testcancel();
}

// Executes the virtual member method
void Threading::pxThread::_try_virtual_invoke(void (pxThread::*method)())
{
    try { (this->*method)(); }
    catch (std::runtime_error &ex) { }
    catch (BaseException &ex) { }
}

// invoked internally when canceling or exiting the thread.  Extending classes should implement
// OnCleanupInThread() to extend cleanup functionality.
void Threading::pxThread::_ThreadCleanup()
{
    _try_virtual_invoke(&pxThread::OnCleanupInThread);
    m_mtx_InThread.Release();

    // Must set m_running LAST, as thread destructors depend on this value (it is used
    // to avoid destruction of the thread until all internal data use has stopped.
    m_running = false;
}

// This override is called by PeristentThread when the thread is first created, prior to
// calling ExecuteTaskInThread, and after the initial InThread lock has been claimed.
// This code is also executed within a "safe" environment, where the creating thread is
// blocked against m_sem_event.  Make sure to do any necessary variable setup here, without
// worry that the calling thread might attempt to test the status of those variables
// before initialization has completed.
//
void Threading::pxThread::OnStartInThread()
{
    m_detached = false;
    m_running  = true;
}

void Threading::pxThread::_internal_execute()
{
    m_mtx_InThread.Acquire();

    make_curthread_key(this);
    if (curthread_key)
        pthread_setspecific(curthread_key, this);

    OnStartInThread();
    m_sem_startup.Post();

    _try_virtual_invoke(&pxThread::ExecuteTaskInThread);
}

// Called by Start, prior to actual starting of the thread, and after any previous
// running thread has been canceled or detached.
void Threading::pxThread::OnStart()
{
    m_mtx_InThread.Acquire();
    m_mtx_InThread.Release();
    m_sem_event.Reset();
    m_sem_startup.Reset();
}

// Extending classes that override this method should always call it last from their
// personal implementations.
void Threading::pxThread::OnCleanupInThread()
{
    if (curthread_key)
        pthread_setspecific(curthread_key, NULL);

    unmake_curthread_key();
    m_evtsrc_OnDelete.Dispatch(0);
}

#ifdef _WIN32
__forceinline void Threading::sleep(int ms)
{
    Sleep(ms);
}

// For use in spin/wait loops,  Acts as a hint to Intel CPUs and should, in theory
// improve performance and reduce cpu power consumption.
__forceinline void Threading::SpinWait()
{
    _mm_pause();
}
#else
// Note: assuming multicore is safer because it forces the interlocked routines to use
// the LOCK prefix.  The prefix works on single core CPUs fine (but is slow), but not
// having the LOCK prefix is very bad indeed.

__forceinline void Threading::sleep(int ms)
{
    usleep(1000 * ms);
}

// For use in spin/wait loops,  Acts as a hint to Intel CPUs and should, in theory
// improve performance and reduce cpu power consumption.
__forceinline void Threading::SpinWait()
{
    // If this doesn't compile you can just comment it out (it only serves as a
    // performance hint and isn't required).
    __asm__("pause");
}
#endif
