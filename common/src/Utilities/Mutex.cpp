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

#include <wx/datetime.h>

#include "Dependencies.h"

#include "Threading.h"

namespace Threading
{
static std::atomic<int> _attr_refcount(0);
static pthread_mutexattr_t _attr_recursive;
}

// --------------------------------------------------------------------------------------
//  Mutex Implementations
// --------------------------------------------------------------------------------------

#if defined(_WIN32) || (defined(_POSIX_TIMEOUTS) && _POSIX_TIMEOUTS >= 200112L)
// good, we have pthread_mutex_timedlock
#define xpthread_mutex_timedlock pthread_mutex_timedlock
#else
// We have to emulate pthread_mutex_timedlock(). This could be a serious
// performance drain if its used a lot.

#include <sys/time.h> // gettimeofday()

// sleep for 10ms at a time
#define TIMEDLOCK_EMU_SLEEP_NS 10000000ULL

// Original POSIX docs:
//
// The pthread_mutex_timedlock() function shall lock the mutex object
// referenced by mutex. If the mutex is already locked, the calling thread
// shall block until the mutex becomes available as in the
// pthread_mutex_lock() function. If the mutex cannot be locked without
// waiting for another thread to unlock the mutex, this wait shall be
// terminated when the specified timeout expires.
//
// This is an implementation that emulates pthread_mutex_timedlock() via
// pthread_mutex_trylock().
static int xpthread_mutex_timedlock(
    pthread_mutex_t *mutex,
    const struct timespec *abs_timeout)
{
    int err = 0;

    while ((err = pthread_mutex_trylock(mutex)) == EBUSY) {
        // check if the timeout has expired, gettimeofday() is implemented
        // efficiently (in userspace) on OSX
        struct timeval now;
        gettimeofday(&now, NULL);
        if (now.tv_sec > abs_timeout->tv_sec
            || (now.tv_sec == abs_timeout->tv_sec
                && (u64)now.tv_usec * 1000ULL > (u64)abs_timeout->tv_nsec)) {
            return ETIMEDOUT;
        }

        // acquiring lock failed, sleep some
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = TIMEDLOCK_EMU_SLEEP_NS;
        while (nanosleep(&ts, &ts) == -1);
    }

    return err;
}
#endif

Threading::Mutex::Mutex()
{
    pthread_mutex_init(&m_mutex, NULL);
}

void Threading::Mutex::Detach()
{
    if (pthread_mutex_destroy(&m_mutex) != EBUSY)
        return;

    if (IsRecursive()) {
        // Sanity check: Recursive locks could be held by our own thread, which would
        // be considered an assertion failure, but can also be handled gracefully.
        // (note: if the mutex is locked recursively more than twice then this assert won't
        //  detect it)

        Release();
        Release(); // in case of double recursion.
        if (pthread_mutex_destroy(&m_mutex) != EBUSY)
            return;
    }

    Release();
    pthread_mutex_destroy(&m_mutex);
}

Threading::Mutex::~Mutex()
{
	Mutex::Detach();
}

Threading::MutexRecursive::MutexRecursive()
    : Mutex(false)
{
    if (++_attr_refcount == 1)
    {
        pthread_mutexattr_init(&_attr_recursive);
        pthread_mutexattr_settype(&_attr_recursive, PTHREAD_MUTEX_RECURSIVE);
    }

    pthread_mutex_init(&m_mutex, &_attr_recursive);
}

Threading::MutexRecursive::~MutexRecursive()
{
    if (--_attr_refcount == 0)
        pthread_mutexattr_destroy(&_attr_recursive);
}

void Threading::Mutex::Release()
{
    pthread_mutex_unlock(&m_mutex);
}

bool Threading::Mutex::TryAcquire()
{
    return EBUSY != pthread_mutex_trylock(&m_mutex);
}

void Threading::Mutex::Acquire()
{
    pthread_mutex_lock(&m_mutex);
}

bool Threading::Mutex::WaitWithoutYield(const wxTimeSpan &timeout)
{
    wxDateTime megafail(wxDateTime::UNow() + timeout);
    const timespec fail = {megafail.GetTicks(), megafail.GetMillisecond() * 1000000};
    if (xpthread_mutex_timedlock(&m_mutex, &fail) == 0)
    {
        Release();
        return true;
    }
    return false;
}

// --------------------------------------------------------------------------------------
//  ScopedLock Implementations
// --------------------------------------------------------------------------------------

Threading::ScopedLock::~ScopedLock()
{
    if (m_IsLocked && m_lock)
        m_lock->Release();
}

Threading::ScopedLock::ScopedLock(const Mutex *locker)
{
    m_IsLocked = false;
    AssignAndLock(locker);
}

Threading::ScopedLock::ScopedLock(const Mutex &locker)
{
    m_IsLocked = false;
    AssignAndLock(locker);
}

void Threading::ScopedLock::AssignAndLock(const Mutex &locker)
{
    AssignAndLock(&locker);
}

void Threading::ScopedLock::AssignAndLock(const Mutex *locker)
{
    m_lock = const_cast<Mutex *>(locker);
    if (!m_lock)
        return;

    m_IsLocked = true;
    m_lock->Acquire();
}

// Provides manual unlocking of a scoped lock prior to object destruction.
void Threading::ScopedLock::Release()
{
    if (!m_IsLocked)
        return;
    m_IsLocked = false;
    if (m_lock)
        m_lock->Release();
}

// provides manual locking of a scoped lock, to re-lock after a manual unlocking.
void Threading::ScopedLock::Acquire()
{
    if (m_IsLocked || !m_lock)
        return;
    m_lock->Acquire();
    m_IsLocked = true;
}

Threading::ScopedLock::ScopedLock(const Mutex &locker, bool isTryLock)
{
    m_lock = const_cast<Mutex *>(&locker);
    if (!m_lock)
        return;
    m_IsLocked = isTryLock ? m_lock->TryAcquire() : false;
}
