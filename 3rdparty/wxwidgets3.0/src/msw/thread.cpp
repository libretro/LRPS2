/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/thread.cpp
// Purpose:     wxThread Implementation
// Author:      Original from Wolfram Gloger/Guilhem Lavaux
// Modified by: Vadim Zeitlin to make it work :-)
// Created:     04/22/98
// Copyright:   (c) Wolfram Gloger (1996, 1997), Guilhem Lavaux (1998);
//                  Vadim Zeitlin (1999-2002)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_THREADS

#include "wx/thread.h"

#ifndef WX_PRECOMP
    #include "wx/event.h"
    #include "wx/app.h"
    #include "wx/module.h"
#endif

#include "wx/apptrait.h"
#include "wx/evtloop.h"
#include "wx/crt.h"
#include "wx/scopeguard.h"

#include "wx/msw/private.h"
#include "wx/msw/missing.h"

// must have this symbol defined to get _beginthread/_endthread declarations
#ifndef _MT
    #define _MT
#endif

// define wxUSE_BEGIN_THREAD if the compiler has _beginthreadex() function
// which should be used instead of Win32 ::CreateThread() if possible
#if defined(__VISUALC__) || \
    (defined(__GNUG__) && defined(__MSVCRT__))

#undef wxUSE_BEGIN_THREAD
#define wxUSE_BEGIN_THREAD

#endif

#ifdef wxUSE_BEGIN_THREAD
    // this is where _beginthreadex() is declared
    #include <process.h>

    // the return type of the thread function entry point: notice that this
    // type can't hold a pointer under Win64
    typedef unsigned THREAD_RETVAL;

    // the calling convention of the thread function entry point
    #define THREAD_CALLCONV __stdcall
#else
    // the settings for CreateThread()
    typedef DWORD THREAD_RETVAL;
    #define THREAD_CALLCONV WINAPI
#endif

static const THREAD_RETVAL THREAD_ERROR_EXIT = (THREAD_RETVAL)-1;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the possible states of the thread ("=>" shows all possible transitions from
// this state)
enum wxThreadState
{
    STATE_NEW,          // didn't start execution yet (=> RUNNING)
    STATE_RUNNING,      // thread is running (=> PAUSED, CANCELED)
    STATE_PAUSED,       // thread is temporarily suspended (=> RUNNING)
    STATE_CANCELED,     // thread should terminate a.s.a.p. (=> EXITED)
    STATE_EXITED        // thread is terminating
};

// ----------------------------------------------------------------------------
// this module globals
// ----------------------------------------------------------------------------

// TLS index of the slot where we store the pointer to the current thread
static DWORD gs_tlsThisThread = 0xFFFFFFFF;

// id of the main thread - the one which can call GUI functions without first
// calling wxMutexGuiEnter()
wxThreadIdType wxThread::ms_idMainThread = 0;

// ============================================================================
// Windows implementation of thread and related classes
// ============================================================================

// ----------------------------------------------------------------------------
// wxCriticalSection
// ----------------------------------------------------------------------------

wxCriticalSection::wxCriticalSection( wxCriticalSectionType WXUNUSED(critSecType) )
{
    ::InitializeCriticalSection((CRITICAL_SECTION *)m_buffer);
}

wxCriticalSection::~wxCriticalSection()
{
    ::DeleteCriticalSection((CRITICAL_SECTION *)m_buffer);
}

void wxCriticalSection::Enter()
{
    ::EnterCriticalSection((CRITICAL_SECTION *)m_buffer);
}

bool wxCriticalSection::TryEnter()
{
    return false;
}

void wxCriticalSection::Leave()
{
    ::LeaveCriticalSection((CRITICAL_SECTION *)m_buffer);
}

// ----------------------------------------------------------------------------
// wxMutex
// ----------------------------------------------------------------------------

class wxMutexInternal
{
public:
    wxMutexInternal(wxMutexType mutexType);
    ~wxMutexInternal();

    bool IsOk() const { return m_mutex != NULL; }

    wxMutexError Lock() { return LockTimeout(INFINITE); }
    wxMutexError Lock(unsigned long ms) { return LockTimeout(ms); }
    wxMutexError TryLock();
    wxMutexError Unlock();

private:
    wxMutexError LockTimeout(DWORD milliseconds);

    HANDLE m_mutex;

    unsigned long m_owningThread;
    wxMutexType m_type;

    wxDECLARE_NO_COPY_CLASS(wxMutexInternal);
};

// all mutexes are recursive under Win32 so we don't use mutexType
wxMutexInternal::wxMutexInternal(wxMutexType mutexType)
{
    // create a nameless (hence intra process and always private) mutex
    m_mutex = ::CreateMutex
                (
                    NULL,       // default secutiry attributes
                    FALSE,      // not initially locked
                    NULL        // no name
                );

    m_type = mutexType;
    m_owningThread = 0;
}

wxMutexInternal::~wxMutexInternal()
{
    if ( m_mutex )
    {
        if ( !::CloseHandle(m_mutex) ) { }
    }
}

wxMutexError wxMutexInternal::TryLock()
{
    const wxMutexError rc = LockTimeout(0);

    // we have a special return code for timeout in this case
    return rc == wxMUTEX_TIMEOUT ? wxMUTEX_BUSY : rc;
}

wxMutexError wxMutexInternal::LockTimeout(DWORD milliseconds)
{
    if (m_type == wxMUTEX_DEFAULT)
    {
        // Don't allow recursive
        if (m_owningThread != 0)
        {
            if (m_owningThread == wxThread::GetCurrentId())
                return wxMUTEX_DEAD_LOCK;
        }
    }

    DWORD rc = ::WaitForSingleObject(m_mutex, milliseconds);
    switch ( rc )
    {
        case WAIT_ABANDONED:
            // the previous caller died without releasing the mutex, so even
            // though we did get it, log a message about this
            // fall through

        case WAIT_OBJECT_0:
            // ok
            break;

        case WAIT_TIMEOUT:
            return wxMUTEX_TIMEOUT;

        default:
            // fall through

        case WAIT_FAILED:
            return wxMUTEX_MISC_ERROR;
    }

    if (m_type == wxMUTEX_DEFAULT)
    {
        // required for checking recursiveness
        m_owningThread = wxThread::GetCurrentId();
    }

    return wxMUTEX_NO_ERROR;
}

wxMutexError wxMutexInternal::Unlock()
{
    // required for checking recursiveness
    m_owningThread = 0;

    if ( !::ReleaseMutex(m_mutex) )
        return wxMUTEX_MISC_ERROR;

    return wxMUTEX_NO_ERROR;
}

// --------------------------------------------------------------------------
// wxSemaphore
// --------------------------------------------------------------------------

// a trivial wrapper around Win32 semaphore
class wxSemaphoreInternal
{
public:
    wxSemaphoreInternal(int initialcount, int maxcount);
    ~wxSemaphoreInternal();

    bool IsOk() const { return m_semaphore != NULL; }

    wxSemaError Wait() { return WaitTimeout(INFINITE); }

    wxSemaError TryWait()
    {
        wxSemaError rc = WaitTimeout(0);
        if ( rc == wxSEMA_TIMEOUT )
            rc = wxSEMA_BUSY;

        return rc;
    }

    wxSemaError WaitTimeout(unsigned long milliseconds);

    wxSemaError Post();

private:
    HANDLE m_semaphore;

    wxDECLARE_NO_COPY_CLASS(wxSemaphoreInternal);
};

wxSemaphoreInternal::wxSemaphoreInternal(int initialcount, int maxcount)
{
#if !defined(_WIN32_WCE) || (_WIN32_WCE >= 300)
    if ( maxcount == 0 )
    {
        // make it practically infinite
        maxcount = INT_MAX;
    }

    m_semaphore = ::CreateSemaphore
                    (
                        NULL,           // default security attributes
                        initialcount,
                        maxcount,
                        NULL            // no name
                    );
#endif
}

wxSemaphoreInternal::~wxSemaphoreInternal()
{
    if ( m_semaphore )
    {
        if ( !::CloseHandle(m_semaphore) ) { }
    }
}

wxSemaError wxSemaphoreInternal::WaitTimeout(unsigned long milliseconds)
{
    DWORD rc = ::WaitForSingleObject( m_semaphore, milliseconds );

    switch ( rc )
    {
        case WAIT_OBJECT_0:
           return wxSEMA_NO_ERROR;

        case WAIT_TIMEOUT:
           return wxSEMA_TIMEOUT;

        default:
	   break;
    }

    return wxSEMA_MISC_ERROR;
}

wxSemaError wxSemaphoreInternal::Post()
{
#if !defined(_WIN32_WCE) || (_WIN32_WCE >= 300)
    if ( !::ReleaseSemaphore(m_semaphore, 1, NULL /* ptr to previous count */) )
    {
	    if ( GetLastError() == ERROR_TOO_MANY_POSTS )
		    return wxSEMA_OVERFLOW;
	    return wxSEMA_MISC_ERROR;
    }

    return wxSEMA_NO_ERROR;
#else
    return wxSEMA_MISC_ERROR;
#endif
}

// ----------------------------------------------------------------------------
// wxThread implementation
// ----------------------------------------------------------------------------

// wxThreadInternal class
// ----------------------

class wxThreadInternal
{
public:
    wxThreadInternal(wxThread *thread)
    {
        m_thread = thread;
        m_hThread = 0;
        m_state = STATE_NEW;
        m_priority = wxPRIORITY_DEFAULT;
        m_nRef = 1;
    }

    ~wxThreadInternal()
    {
        Free();
    }

    void Free()
    {
        if ( m_hThread )
        {
            if ( !::CloseHandle(m_hThread) ) { }

            m_hThread = 0;
        }
    }

    // create a new (suspended) thread (for the given thread object)
    bool Create(wxThread *thread, unsigned int stackSize);

    // wait for the thread to terminate, either by itself, or by asking it
    // (politely, this is not Kill()!) to do it
    wxThreadError WaitForTerminate(wxCriticalSection& cs,
                                   wxThread::ExitCode *pRc,
                                   wxThreadWait waitMode,
                                   wxThread *threadToDelete = NULL);

    // kill the thread unconditionally
    wxThreadError Kill();

    // suspend/resume/terminate
    bool Suspend();
    bool Resume();
    void Cancel() { m_state = STATE_CANCELED; }

    // thread state
    void SetState(wxThreadState state) { m_state = state; }
    wxThreadState GetState() const { return m_state; }

    // thread priority
    void SetPriority(unsigned int priority);
    unsigned int GetPriority() const { return m_priority; }

    // thread handle and id
    HANDLE GetHandle() const { return m_hThread; }
    DWORD  GetId() const { return m_tid; }

    // the thread function forwarding to DoThreadStart
    static THREAD_RETVAL THREAD_CALLCONV WinThreadStart(void *thread);

    // really start the thread (if it's not already dead)
    static THREAD_RETVAL DoThreadStart(wxThread *thread);

    // call OnExit() on the thread
    static void DoThreadOnExit(wxThread *thread);


    void KeepAlive()
    {
        if ( m_thread->IsDetached() )
            ::InterlockedIncrement(&m_nRef);
    }

    void LetDie()
    {
        if ( m_thread->IsDetached() && !::InterlockedDecrement(&m_nRef) )
            delete m_thread;
    }

private:
    // the thread we're associated with
    wxThread *m_thread;

    HANDLE        m_hThread;    // handle of the thread
    wxThreadState m_state;      // state, see wxThreadState enum
    unsigned int  m_priority;   // thread priority in "wx" units
    DWORD         m_tid;        // thread id

    // number of threads which need this thread to remain alive, when the count
    // reaches 0 we kill the owning wxThread -- and die ourselves with it
    LONG m_nRef;

    wxDECLARE_NO_COPY_CLASS(wxThreadInternal);
};

// small class which keeps a thread alive during its lifetime
class wxThreadKeepAlive
{
public:
    wxThreadKeepAlive(wxThreadInternal& thrImpl) : m_thrImpl(thrImpl)
        { m_thrImpl.KeepAlive(); }
    ~wxThreadKeepAlive()
        { m_thrImpl.LetDie(); }

private:
    wxThreadInternal& m_thrImpl;
};

/* static */
void wxThreadInternal::DoThreadOnExit(wxThread *thread)
{
	thread->OnExit();
}

/* static */
THREAD_RETVAL wxThreadInternal::DoThreadStart(wxThread *thread)
{
	wxON_BLOCK_EXIT1(DoThreadOnExit, thread);

	// store the thread object in the TLS
	if ( !::TlsSetValue(gs_tlsThisThread, thread) )
		return THREAD_ERROR_EXIT;

	return wxPtrToUInt(thread->CallEntry());
}

/* static */
THREAD_RETVAL THREAD_CALLCONV wxThreadInternal::WinThreadStart(void *param)
{
    THREAD_RETVAL rc = THREAD_ERROR_EXIT;

    wxThread * const thread = (wxThread *)param;

    // NB: Notice that we can't use wxCriticalSectionLocker in this function as
    //     we use SEH and it's incompatible with C++ object dtors.

    // first of all, check whether we hadn't been cancelled already and don't
    // start the user code at all then
    thread->m_critsect.Enter();
    const bool hasExited = thread->m_internal->GetState() == STATE_EXITED;
    thread->m_critsect.Leave();

    if ( hasExited )
	    DoThreadOnExit(thread);
    else
	    rc = DoThreadStart(thread);

    // save IsDetached because thread object can be deleted by joinable
    // threads after state is changed to STATE_EXITED.
    const bool isDetached = thread->IsDetached();
    if ( !hasExited )
    {
        thread->m_critsect.Enter();
        thread->m_internal->SetState(STATE_EXITED);
        thread->m_critsect.Leave();
    }

    // the thread may delete itself now if it wants, we don't need it any more
    if ( isDetached )
        thread->m_internal->LetDie();

    return rc;
}

void wxThreadInternal::SetPriority(unsigned int priority)
{
    m_priority = priority;

    // translate wxWidgets priority to the Windows one
    int win_priority;
    if (m_priority <= 20)
        win_priority = THREAD_PRIORITY_LOWEST;
    else if (m_priority <= 40)
        win_priority = THREAD_PRIORITY_BELOW_NORMAL;
    else if (m_priority <= 60)
        win_priority = THREAD_PRIORITY_NORMAL;
    else if (m_priority <= 80)
        win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
    else if (m_priority <= 100)
        win_priority = THREAD_PRIORITY_HIGHEST;
    else
        win_priority = THREAD_PRIORITY_NORMAL;

    if ( !::SetThreadPriority(m_hThread, win_priority) )
    {
    }
}

bool wxThreadInternal::Create(wxThread *thread, unsigned int stackSize)
{
    // for compilers which have it, we should use C RTL function for thread
    // creation instead of Win32 API one because otherwise we will have memory
    // leaks if the thread uses C RTL (and most threads do)
#ifdef wxUSE_BEGIN_THREAD
    m_hThread = (HANDLE)_beginthreadex
                        (
                          NULL,                             // default security
                          stackSize,
                          wxThreadInternal::WinThreadStart, // entry point
                          thread,
                          CREATE_SUSPENDED,
                          (unsigned int *)&m_tid
                        );
#else // compiler doesn't have _beginthreadex
    m_hThread = ::CreateThread
                  (
                    NULL,                               // default security
                    stackSize,                          // stack size
                    wxThreadInternal::WinThreadStart,   // thread entry point
                    (LPVOID)thread,                     // parameter
                    CREATE_SUSPENDED,                   // flags
                    &m_tid                              // [out] thread id
                  );
#endif // _beginthreadex/CreateThread

    if ( m_hThread == NULL )
        return false;

    if ( m_priority != wxPRIORITY_DEFAULT )
    {
        SetPriority(m_priority);
    }

    return true;
}

wxThreadError wxThreadInternal::Kill()
{
    m_thread->OnKill();

    if ( !::TerminateThread(m_hThread, THREAD_ERROR_EXIT) )
        return wxTHREAD_MISC_ERROR;

    Free();

    return wxTHREAD_NO_ERROR;
}

wxThreadError
wxThreadInternal::WaitForTerminate(wxCriticalSection& cs,
                                   wxThread::ExitCode *pRc,
                                   wxThreadWait waitMode,
                                   wxThread *threadToDelete)
{
    // prevent the thread C++ object from disappearing as long as we are using
    // it here
    wxThreadKeepAlive keepAlive(*this);


    // we may either wait passively for the thread to terminate (when called
    // from Wait()) or ask it to terminate (when called from Delete())
    bool shouldDelete = threadToDelete != NULL;

    DWORD rc = 0;

    // we might need to resume the thread if it's currently stopped
    bool shouldResume = false;

    // as Delete() (which calls us) is always safe to call we need to consider
    // all possible states
    {
        wxCriticalSectionLocker lock(cs);

        if ( m_state == STATE_NEW )
        {
            if ( shouldDelete )
            {
                // WinThreadStart() will see it and terminate immediately, no
                // need to cancel the thread -- but we still need to resume it
                // to let it run
                m_state = STATE_EXITED;

                // we must call Resume() as the thread hasn't been initially
                // resumed yet (and as Resume() it knows about STATE_EXITED
                // special case, it won't touch it and WinThreadStart() will
                // just exit immediately)
                shouldResume = true;
                shouldDelete = false;
            }
            //else: shouldResume is correctly set to false here, wait until
            //      someone else runs the thread and it finishes
        }
        else // running, paused, cancelled or even exited
        {
            shouldResume = m_state == STATE_PAUSED;
        }
    }

    // resume the thread if it is paused
    if ( shouldResume )
        Resume();

    // ask the thread to terminate
    if ( shouldDelete )
    {
        wxCriticalSectionLocker lock(cs);

        Cancel();
    }

    if ( threadToDelete )
        threadToDelete->OnDelete();

    // we can't just wait for the thread to terminate because it might be
    // calling some GUI functions and so it will never terminate before we
    // process the Windows messages that result from these functions
    // (note that even in console applications we might have to process
    // messages if we use wxExecute() or timers or ...)
    DWORD result wxDUMMY_INITIALIZE(0);
    do
    {
        // Wait for the thread while still processing events in the GUI apps or
        // just simply wait for it in the console ones.
	result = WaitForSingleObject((HANDLE)m_hThread, INFINITE);

        switch ( result )
        {
            case 0xFFFFFFFF:
                // error
                Kill();
                return wxTHREAD_KILLED;

            case WAIT_OBJECT_0:
                // thread we're waiting for terminated
                break;

            case WAIT_OBJECT_0 + 1:
                break;

            default:
		break;
        }
    } while ( result != WAIT_OBJECT_0 );

    // although the thread might be already in the EXITED state it might not
    // have terminated yet and so we are not sure that it has actually
    // terminated if the "if" above hadn't been taken
    for ( ;; )
    {
        if ( !::GetExitCodeThread(m_hThread, &rc) )
        {
            rc = THREAD_ERROR_EXIT;

            break;
        }

        if ( rc != STILL_ACTIVE )
            break;

        // give the other thread some time to terminate, otherwise we may be
        // starving it
        ::Sleep(1);
    }

    if ( pRc )
        *pRc = wxUIntToPtr(rc);

    // we don't need the thread handle any more in any case
    Free();


    return rc == THREAD_ERROR_EXIT ? wxTHREAD_MISC_ERROR : wxTHREAD_NO_ERROR;
}

bool wxThreadInternal::Suspend()
{
    DWORD nSuspendCount = ::SuspendThread(m_hThread);
    if ( nSuspendCount == (DWORD)-1 )
        return false;

    m_state = STATE_PAUSED;

    return true;
}

bool wxThreadInternal::Resume()
{
    DWORD nSuspendCount = ::ResumeThread(m_hThread);
    if ( nSuspendCount == (DWORD)-1 )
        return false;

    // don't change the state from STATE_EXITED because it's special and means
    // we are going to terminate without running any user code - if we did it,
    // the code in WaitForTerminate() wouldn't work
    if ( m_state != STATE_EXITED )
    {
        m_state = STATE_RUNNING;
    }

    return true;
}

// static functions
// ----------------

wxThread *wxThread::This()
{
    wxThread *thread = (wxThread *)::TlsGetValue(gs_tlsThisThread);

    // be careful, 0 may be a valid return value as well
    if ( !thread && (::GetLastError() != NO_ERROR) ) { }

    return thread;
}

unsigned long wxThread::GetCurrentId()
{
    return (unsigned long)::GetCurrentThreadId();
}

// ctor and dtor
// -------------

wxThread::wxThread(wxThreadKind kind)
{
    m_internal = new wxThreadInternal(this);

    m_isDetached = kind == wxTHREAD_DETACHED;
}

wxThread::~wxThread()
{
    delete m_internal;
}

// create/start thread
// -------------------

wxThreadError wxThread::Create(unsigned int stackSize)
{
    wxCriticalSectionLocker lock(m_critsect);

    if ( !m_internal->Create(this, stackSize) )
        return wxTHREAD_NO_RESOURCE;

    return wxTHREAD_NO_ERROR;
}

wxThreadError wxThread::Run()
{
    wxCriticalSectionLocker lock(m_critsect);

    // Create the thread if it wasn't created yet with an explicit
    // Create() call:
    if ( !m_internal->GetHandle() )
    {
        if ( !m_internal->Create(this, 0) )
            return wxTHREAD_NO_RESOURCE;
    }

    // the thread has just been created and is still suspended - let it run
    return Resume();
}

// suspend/resume thread
// ---------------------

wxThreadError wxThread::Pause()
{
    wxCriticalSectionLocker lock(m_critsect);

    return m_internal->Suspend() ? wxTHREAD_NO_ERROR : wxTHREAD_MISC_ERROR;
}

wxThreadError wxThread::Resume()
{
    wxCriticalSectionLocker lock(m_critsect);

    return m_internal->Resume() ? wxTHREAD_NO_ERROR : wxTHREAD_MISC_ERROR;
}

// stopping thread
// ---------------

wxThread::ExitCode wxThread::Wait(wxThreadWait waitMode)
{
    ExitCode rc = wxUIntToPtr(THREAD_ERROR_EXIT);
    (void)m_internal->WaitForTerminate(m_critsect, &rc, waitMode);
    return rc;
}

wxThreadError wxThread::Delete(ExitCode *pRc, wxThreadWait waitMode)
{
    return m_internal->WaitForTerminate(m_critsect, pRc, waitMode, this);
}

wxThreadError wxThread::Kill()
{
    if ( !IsRunning() )
        return wxTHREAD_NOT_RUNNING;

    wxThreadError rc = m_internal->Kill();

    if ( IsDetached() )
        delete this;

    // joinable
    // update the status of the joinable thread
    wxCriticalSectionLocker lock(m_critsect);
    m_internal->SetState(STATE_EXITED);

    return rc;
}

void wxThread::Exit(ExitCode status)
{
    wxThreadInternal::DoThreadOnExit(this);

    m_internal->Free();

    if ( IsDetached() )
    {
        delete this;
    }
    else // joinable
    {
        // update the status of the joinable thread
        wxCriticalSectionLocker lock(m_critsect);
        m_internal->SetState(STATE_EXITED);
    }

#ifdef wxUSE_BEGIN_THREAD
    _endthreadex(wxPtrToUInt(status));
#else // !VC++
    ::ExitThread(wxPtrToUInt(status));
#endif // VC++/!VC++
}

// priority setting
// ----------------

void wxThread::SetPriority(unsigned int prio)
{
    wxCriticalSectionLocker lock(m_critsect);

    m_internal->SetPriority(prio);
}

unsigned int wxThread::GetPriority() const
{
    wxCriticalSectionLocker lock(const_cast<wxCriticalSection &>(m_critsect));

    return m_internal->GetPriority();
}

unsigned long wxThread::GetId() const
{
    wxCriticalSectionLocker lock(const_cast<wxCriticalSection &>(m_critsect));

    return (unsigned long)m_internal->GetId();
}

bool wxThread::IsRunning() const
{
    wxCriticalSectionLocker lock(const_cast<wxCriticalSection &>(m_critsect));

    return m_internal->GetState() == STATE_RUNNING;
}

// ----------------------------------------------------------------------------
// Automatic initialization for thread module
// ----------------------------------------------------------------------------

class wxThreadModule : public wxModule
{
public:
    virtual bool OnInit();
    virtual void OnExit();

private:
    DECLARE_DYNAMIC_CLASS(wxThreadModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxThreadModule, wxModule)

bool wxThreadModule::OnInit()
{
    // allocate TLS index for storing the pointer to the current thread
    gs_tlsThisThread = ::TlsAlloc();
    if ( gs_tlsThisThread == 0xFFFFFFFF )
    {
        // in normal circumstances it will only happen if all other
        // TLS_MINIMUM_AVAILABLE (>= 64) indices are already taken - in other
        // words, this should never happen
        return false;
    }

    // main thread doesn't have associated wxThread object, so store 0 in the
    // TLS instead
    if ( !::TlsSetValue(gs_tlsThisThread, (LPVOID)0) )
    {
        ::TlsFree(gs_tlsThisThread);
        gs_tlsThisThread = 0xFFFFFFFF;

        return false;
    }

    wxThread::ms_idMainThread = wxThread::GetCurrentId();

    return true;
}

void wxThreadModule::OnExit()
{
    if ( !::TlsFree(gs_tlsThisThread) ) { }
}

// ----------------------------------------------------------------------------
// under Windows, these functions are implemented using a critical section and
// not a mutex, so the names are a bit confusing
// ----------------------------------------------------------------------------

void wxMutexGuiEnterImpl()
{
}

void wxMutexGuiLeaveImpl()
{
}

// ----------------------------------------------------------------------------
// include common implementation code
// ----------------------------------------------------------------------------

#include "wx/thrimpl.cpp"

#endif // wxUSE_THREADS

// ============================================================================
// wxAppTraits implementation
// ============================================================================

wxEventLoopBase *wxConsoleAppTraits::CreateEventLoop()
{
#if wxUSE_CONSOLE_EVENTLOOP
    return new wxEventLoop();
#else // !wxUSE_CONSOLE_EVENTLOOP
    return NULL;
#endif // wxUSE_CONSOLE_EVENTLOOP/!wxUSE_CONSOLE_EVENTLOOP
}
