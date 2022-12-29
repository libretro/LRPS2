/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include <cstdio>
#include "Dependencies.h"

#include "Threading.h"
#include <wx/datetime.h>

#ifdef __APPLE__
#include <pthread.h> // pthread_setcancelstate()

#include <sys/time.h> // gettimeofday()

#include <mach/mach.h>
#include <mach/task.h>       // semaphore_create() and semaphore_destroy()
#include <mach/semaphore.h>  // semaphore_*()
#include <mach/mach_error.h> // mach_error_string()
#include <mach/mach_time.h>  // mach_absolute_time()
#endif

// --------------------------------------------------------------------------------------
//  Semaphore Implementations
// --------------------------------------------------------------------------------------
//
#ifdef __APPLE__
// --------------------------------------------------------------------------------------
//  Semaphore Implementation for Darwin/OSX
//
//  Sadly, Darwin/OSX needs its own implementation of Semaphores instead of
//  relying on phtreads, because OSX unnamed semaphore (the best kind)
//  support is very poor.
//
//  This implementation makes use of Mach primitives instead. These are also
//  what Grand Central Dispatch (GCD) is based on.
// --------------------------------------------------------------------------------------

static void MACH_CHECK(kern_return_t mach_retval)
{
    switch (mach_retval)
    {
        case KERN_SUCCESS: break;
        case KERN_ABORTED: // Awoken due reason unrelated to semaphore (e.g. pthread_cancel)
            pthread_testcancel(); // Unlike sem_wait, mach semaphore ops aren't cancellation points
            // fallthrough
        default:
	    break;
    }
}
#endif

Threading::Semaphore::Semaphore()
{
#ifdef __APPLE__
    // other platforms explicitly make a thread-private (unshared) semaphore
    // here. But it seems Mach doesn't support that.
    MACH_CHECK(semaphore_create(mach_task_self(), (semaphore_t *)&m_sema, SYNC_POLICY_FIFO, 0));
    __atomic_store_n(&m_counter, 0, __ATOMIC_SEQ_CST);
#else
    sem_init(&m_sema, false, 0);
#endif
}

Threading::Semaphore::~Semaphore()
{
#ifdef __APPLE__
    MACH_CHECK(semaphore_destroy(mach_task_self(), (semaphore_t)m_sema));
    __atomic_store_n(&m_counter, 0, __ATOMIC_SEQ_CST);
#else
    sem_destroy(&m_sema);
#endif
}

void Threading::Semaphore::Reset()
{
#ifdef __APPLE__
    MACH_CHECK(semaphore_destroy(mach_task_self(), (semaphore_t)m_sema));
    MACH_CHECK(semaphore_create(mach_task_self(), (semaphore_t *)&m_sema, SYNC_POLICY_FIFO, 0));
    __atomic_store_n(&m_counter, 0, __ATOMIC_SEQ_CST);
#else
    sem_destroy(&m_sema);
    sem_init(&m_sema, false, 0);
#endif
}

void Threading::Semaphore::Post()
{
#ifdef __APPLE__
    MACH_CHECK(semaphore_signal(m_sema));
    __atomic_add_fetch(&m_counter, 1, __ATOMIC_SEQ_CST);
#else
    sem_post(&m_sema);
#endif
}

void Threading::Semaphore::Post(int multiple)
{
#if defined(_MSC_VER)
    sem_post_multiple(&m_sema, multiple);
#elif defined(__APPLE__)
    for (int i = 0; i < multiple; ++i) {
        MACH_CHECK(semaphore_signal(m_sema));
    }
    __atomic_add_fetch(&m_counter, multiple, __ATOMIC_SEQ_CST);
#else
    // Only w32pthreads has the post_multiple, but it's easy enough to fake:
    while (multiple > 0) {
        multiple--;
        sem_post(&m_sema);
    }
#endif
}

bool Threading::Semaphore::WaitWithoutYield(const wxTimeSpan &timeout)
{
#ifdef __APPLE__
    // This method is the reason why there has to be a special Darwin
    // implementation of Semaphore. Note that semaphore_timedwait() is prone
    // to returning with KERN_ABORTED, which basically signifies that some
    // signal has worken it up. The best official "documentation" for
    // semaphore_timedwait() is the way it's used in Grand Central Dispatch,
    // which is open-source.

    // on x86 platforms, mach_absolute_time() returns nanoseconds
    // TODO(aktau): on iOS a scale value from mach_timebase_info will be necessary
    u64 const kOneThousand = 1000;
    u64 const kOneBillion = kOneThousand * kOneThousand * kOneThousand;
    u64 const delta = timeout.GetMilliseconds().GetValue() * (kOneThousand * kOneThousand);
    mach_timespec_t ts;
    kern_return_t kr = KERN_ABORTED;
    for (u64 now = mach_absolute_time(), deadline = now + delta;
         kr == KERN_ABORTED; now = mach_absolute_time()) {
        // timed out by definition
        if (now > deadline)
            return false;

        u64 timeleft = deadline - now;
        ts.tv_sec = timeleft / kOneBillion;
        ts.tv_nsec = timeleft % kOneBillion;

        // possible return values of semaphore_timedwait() (from XNU sources):
        // internal kernel val -> return value
        // THREAD_INTERRUPTED  -> KERN_ABORTED
        // THREAD_TIMED_OUT    -> KERN_OPERATION_TIMED_OUT
        // THREAD_AWAKENED     -> KERN_SUCCESS
        // THREAD_RESTART      -> KERN_TERMINATED
        // default             -> KERN_FAILURE
        kr = semaphore_timedwait(m_sema, ts);
    }

    if (kr == KERN_OPERATION_TIMED_OUT)
        return false;

    // while it's entirely possible to have KERN_FAILURE here, we should
    // probably assert so we can study and correct the actual error here
    // (the thread dying while someone is wainting for it).
    MACH_CHECK(kr);

    __atomic_sub_fetch(&m_counter, 1, __ATOMIC_SEQ_CST);
    return true;
#else
    wxDateTime megafail(wxDateTime::UNow() + timeout);
    const timespec fail = {megafail.GetTicks(), megafail.GetMillisecond() * 1000000};
    return sem_timedwait(&m_sema, &fail) == 0;
#endif
}

void Threading::Semaphore::Wait()
{
#ifdef __APPLE__
    MACH_CHECK(semaphore_wait(m_sema));
    __atomic_sub_fetch(&m_counter, 1, __ATOMIC_SEQ_CST);
#else
    sem_wait(&m_sema);
#endif
}

// Performs an uncancellable wait on a semaphore; restoring the thread's previous cancel state
// after the wait has completed.  Useful for situations where the semaphore itself is stored on
// the stack and passed to another thread via GUI message or such, avoiding complications where
// the thread might be canceled and the stack value becomes invalid.
//
// Performance note: this function has quite a bit more overhead compared to Semaphore::WaitWithoutYield(), so
// consider manually specifying the thread as uncancellable and using WaitWithoutYield() instead if you need
// to do a lot of no-cancel waits in a tight loop worker thread, for example.
void Threading::Semaphore::WaitNoCancel()
{
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    Wait();
    pthread_setcancelstate(oldstate, NULL);
}

int Threading::Semaphore::Count()
{
#ifdef __APPLE__
    return __atomic_load_n(&m_counter, __ATOMIC_SEQ_CST);
#else
    int retval;
    sem_getvalue(&m_sema, &retval);
    return retval;
#endif
}
