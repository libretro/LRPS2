///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/appbase.cpp
// Purpose:     implements wxAppConsoleBase class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.06.2003 (extracted from common/appcmn.cpp)
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #ifdef __WINDOWS__
        #include  "wx/msw/wrapwin.h"  // includes windows.h for MessageBox()
    #endif
    #include "wx/list.h"
    #include "wx/app.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/wxcrtvararg.h"
#endif //WX_PRECOMP

#include "wx/apptrait.h"
#include "wx/confbase.h"
#include "wx/evtloop.h"
#include "wx/filename.h"
#include "wx/scopedptr.h"
#include "wx/tokenzr.h"
#include "wx/thread.h"

#if !defined(__WINDOWS__) || defined(__WXMICROWIN__)
  #include  <signal.h>      // for SIGTRAP used by wxTrap()
#endif  //Win/Unix

#include <locale.h>

// wxABI_VERSION can be defined when compiling applications but it should be
// left undefined when compiling the library itself, it is then set to its
// default value in version.h
#if wxABI_VERSION != wxMAJOR_VERSION * 10000 + wxMINOR_VERSION * 100 + 99
#error "wxABI_VERSION should not be defined when compiling the library"
#endif

// ----------------------------------------------------------------------------
// global vars
// ----------------------------------------------------------------------------

wxAppConsole *wxAppConsoleBase::ms_appInstance = NULL;

wxAppInitializerFunction wxAppConsoleBase::ms_appInitFn = NULL;

wxSocketManager *wxAppTraitsBase::ms_manager = NULL;

WXDLLIMPEXP_DATA_BASE(wxList) wxPendingDelete;

// ----------------------------------------------------------------------------
// wxEventLoopPtr
// ----------------------------------------------------------------------------

// this defines wxEventLoopPtr
wxDEFINE_TIED_SCOPED_PTR_TYPE(wxEventLoopBase)

// ============================================================================
// wxAppConsoleBase implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ctor/dtor
// ----------------------------------------------------------------------------

wxAppConsoleBase::wxAppConsoleBase()
{
    m_traits = NULL;
    m_mainLoop = NULL;
    m_bDoPendingEventProcessing = true;

    ms_appInstance = static_cast<wxAppConsole *>(this);

#ifdef __WXDEBUG__
    SetTraceMasks();
#if wxUSE_UNICODE
    // In unicode mode the SetTraceMasks call can cause an apptraits to be
    // created, but since we are still in the constructor the wrong kind will
    // be created for GUI apps.  Destroy it so it can be created again later.
    wxDELETE(m_traits);
#endif
#endif

    wxEvtHandler::AddFilter(this);
}

wxAppConsoleBase::~wxAppConsoleBase()
{
    wxEvtHandler::RemoveFilter(this);

    // we're being destroyed and using this object from now on may not work or
    // even crash so don't leave dangling pointers to it
    ms_appInstance = NULL;

    delete m_traits;
}

// ----------------------------------------------------------------------------
// initialization/cleanup
// ----------------------------------------------------------------------------

bool wxAppConsoleBase::Initialize(int& WXUNUSED(argc), wxChar **WXUNUSED(argv))
{
#if defined(__WINDOWS__) && !defined(__WXWINCE__)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#endif

    return true;
}

wxString wxAppConsoleBase::GetAppName() const
{
    wxString name = m_appName;
    if ( name.empty() )
    {
        if ( argv )
        {
            // the application name is, by default, the name of its executable file
            wxFileName::SplitPath(argv[0], NULL, &name, NULL);
        }
    }
    return name;
}

wxEventLoopBase *wxAppConsoleBase::CreateMainLoop()
{
    return GetTraits()->CreateEventLoop();
}

void wxAppConsoleBase::CleanUp()
{
    wxDELETE(m_mainLoop);
}

// ----------------------------------------------------------------------------
// OnXXX() callbacks
// ----------------------------------------------------------------------------

bool wxAppConsoleBase::OnInit()
{
    return true;
}

int wxAppConsoleBase::OnRun()
{
    return MainLoop();
}

void wxAppConsoleBase::OnLaunched()
{    
}

int wxAppConsoleBase::OnExit()
{
#if wxUSE_CONFIG
    // delete the config object if any (don't use Get() here, but Set()
    // because Get() could create a new config object)
    delete wxConfigBase::Set(NULL);
#endif // wxUSE_CONFIG

    return 0;
}

void wxAppConsoleBase::Exit()
{
    if (m_mainLoop != NULL)
        ExitMainLoop();
    else
        exit(-1);
}

// ----------------------------------------------------------------------------
// traits stuff
// ----------------------------------------------------------------------------

wxAppTraits *wxAppConsoleBase::CreateTraits()
{
    return new wxConsoleAppTraits;
}

wxAppTraits *wxAppConsoleBase::GetTraits()
{
    // FIXME-MT: protect this with a CS?
    if ( !m_traits )
    {
        m_traits = CreateTraits();

        wxASSERT_MSG( m_traits, wxT("wxApp::CreateTraits() failed?") );
    }

    return m_traits;
}

/* static */
wxAppTraits *wxAppConsoleBase::GetTraitsIfExists()
{
    wxAppConsole * const app = GetInstance();
    return app ? app->GetTraits() : NULL;
}

/* static */
wxAppTraits& wxAppConsoleBase::GetValidTraits()
{
    static wxConsoleAppTraits s_traitsConsole;
    wxAppTraits* const traits = (wxTheApp ? wxTheApp->GetTraits() : NULL);

    return *(traits ? traits : &s_traitsConsole);
}

// ----------------------------------------------------------------------------
// wxEventLoop redirection
// ----------------------------------------------------------------------------

int wxAppConsoleBase::MainLoop()
{
    wxEventLoopBaseTiedPtr mainLoop(&m_mainLoop, CreateMainLoop());

    if (wxTheApp)
        wxTheApp->OnLaunched();
    
    return m_mainLoop ? m_mainLoop->Run() : -1;
}

void wxAppConsoleBase::ExitMainLoop()
{
    // we should exit from the main event loop, not just any currently active
    // (e.g. modal dialog) event loop
    if ( m_mainLoop && m_mainLoop->IsRunning() )
    {
        m_mainLoop->Exit(0);
    }
}

bool wxAppConsoleBase::Pending()
{
    // use the currently active message loop here, not m_mainLoop, because if
    // we're showing a modal dialog (with its own event loop) currently the
    // main event loop is not running anyhow
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    return loop && loop->Pending();
}

bool wxAppConsoleBase::Dispatch()
{
    // see comment in Pending()
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    return loop && loop->Dispatch();
}

void wxAppConsoleBase::WakeUpIdle()
{
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    if ( loop )
        loop->WakeUp();
}

bool wxAppConsoleBase::ProcessIdle()
{
    // synthesize an idle event and check if more of them are needed
    wxIdleEvent event;
    event.SetEventObject(this);
    ProcessEvent(event);

    // Garbage collect all objects previously scheduled for destruction.
    DeletePendingObjects();

    return event.MoreRequested();
}

// ----------------------------------------------------------------------------
// events
// ----------------------------------------------------------------------------

int wxAppConsoleBase::FilterEvent(wxEvent& WXUNUSED(event))
{
    // process the events normally by default
    return Event_Skip;
}

void wxAppConsoleBase::DelayPendingEventHandler(wxEvtHandler* toDelay)
{
    wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

    // move the handler from the list of handlers with processable pending events
    // to the list of handlers with pending events which needs to be processed later
    m_handlersWithPendingEvents.Remove(toDelay);

    if (m_handlersWithPendingDelayedEvents.Index(toDelay) == wxNOT_FOUND)
        m_handlersWithPendingDelayedEvents.Add(toDelay);

    wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
}

void wxAppConsoleBase::RemovePendingEventHandler(wxEvtHandler* toRemove)
{
    wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

    if (m_handlersWithPendingEvents.Index(toRemove) != wxNOT_FOUND)
    {
        m_handlersWithPendingEvents.Remove(toRemove);

        // check that the handler was present only once in the list
        wxASSERT_MSG( m_handlersWithPendingEvents.Index(toRemove) == wxNOT_FOUND,
                        "Handler occurs twice in the m_handlersWithPendingEvents list!" );
    }
    //else: it wasn't in this list at all, it's ok

    if (m_handlersWithPendingDelayedEvents.Index(toRemove) != wxNOT_FOUND)
    {
        m_handlersWithPendingDelayedEvents.Remove(toRemove);

        // check that the handler was present only once in the list
        wxASSERT_MSG( m_handlersWithPendingDelayedEvents.Index(toRemove) == wxNOT_FOUND,
                        "Handler occurs twice in m_handlersWithPendingDelayedEvents list!" );
    }
    //else: it wasn't in this list at all, it's ok

    wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
}

void wxAppConsoleBase::AppendPendingEventHandler(wxEvtHandler* toAppend)
{
    wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

    if ( m_handlersWithPendingEvents.Index(toAppend) == wxNOT_FOUND )
        m_handlersWithPendingEvents.Add(toAppend);

    wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
}

bool wxAppConsoleBase::HasPendingEvents() const
{
    wxENTER_CRIT_SECT(const_cast<wxAppConsoleBase*>(this)->m_handlersWithPendingEventsLocker);

    bool has = !m_handlersWithPendingEvents.IsEmpty();

    wxLEAVE_CRIT_SECT(const_cast<wxAppConsoleBase*>(this)->m_handlersWithPendingEventsLocker);

    return has;
}

void wxAppConsoleBase::ProcessPendingEvents()
{
    if ( m_bDoPendingEventProcessing )
    {
        wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

        wxCHECK_RET( m_handlersWithPendingDelayedEvents.IsEmpty(),
                     "this helper list should be empty" );

        // iterate until the list becomes empty: the handlers remove themselves
        // from it when they don't have any more pending events
        while (!m_handlersWithPendingEvents.IsEmpty())
        {
            // In ProcessPendingEvents(), new handlers might be added
            // and we can safely leave the critical section here.
            wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);

            // NOTE: we always call ProcessPendingEvents() on the first event handler
            //       with pending events because handlers auto-remove themselves
            //       from this list (see RemovePendingEventHandler) if they have no
            //       more pending events.
            m_handlersWithPendingEvents[0]->ProcessPendingEvents();

            wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);
        }

        // now the wxHandlersWithPendingEvents is surely empty; however some event
        // handlers may have moved themselves into wxHandlersWithPendingDelayedEvents
        // because of a selective wxYield call in progress.
        // Now we need to move them back to wxHandlersWithPendingEvents so the next
        // call to this function has the chance of processing them:
        if (!m_handlersWithPendingDelayedEvents.IsEmpty())
        {
            WX_APPEND_ARRAY(m_handlersWithPendingEvents, m_handlersWithPendingDelayedEvents);
            m_handlersWithPendingDelayedEvents.Clear();
        }

        wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
    }
}

void wxAppConsoleBase::DeletePendingEvents()
{
    wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

    wxCHECK_RET( m_handlersWithPendingDelayedEvents.IsEmpty(),
                 "this helper list should be empty" );

    for (unsigned int i=0; i<m_handlersWithPendingEvents.GetCount(); i++)
        m_handlersWithPendingEvents[i]->DeletePendingEvents();

    m_handlersWithPendingEvents.Clear();

    wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
}

// ----------------------------------------------------------------------------
// delayed objects destruction
// ----------------------------------------------------------------------------

void wxAppConsoleBase::DeletePendingObjects()
{
    wxList::compatibility_iterator node = wxPendingDelete.GetFirst();
    while (node)
    {
        wxObject *obj = node->GetData();

        // remove it from the list first so that if we get back here somehow
        // during the object deletion (e.g. wxYield called from its dtor) we
        // wouldn't try to delete it the second time
        if ( wxPendingDelete.Member(obj) )
            wxPendingDelete.Erase(node);

        delete obj;

        // Deleting one object may have deleted other pending
        // objects, so start from beginning of list again.
        node = wxPendingDelete.GetFirst();
    }
}

// ----------------------------------------------------------------------------
// debugging support
// ----------------------------------------------------------------------------

void wxAppConsoleBase::OnAssertFailure(const wxChar *file,
                                       int line,
                                       const wxChar *func,
                                       const wxChar *cond,
                                       const wxChar *msg)
{
    // this function is still present even in debug level 0 build for ABI
    // compatibility reasons but is never called there and so can simply do
    // nothing in it
    wxUnusedVar(file);
    wxUnusedVar(line);
    wxUnusedVar(func);
    wxUnusedVar(cond);
    wxUnusedVar(msg);
}

void wxAppConsoleBase::OnAssert(const wxChar *file,
                                int line,
                                const wxChar *cond,
                                const wxChar *msg)
{
    OnAssertFailure(file, line, NULL, cond, msg);
}

// ----------------------------------------------------------------------------
// Miscellaneous other methods
// ----------------------------------------------------------------------------

void wxAppConsoleBase::SetCLocale()
{
    // We want to use the user locale by default in GUI applications in order
    // to show the numbers, dates &c in the familiar format -- and also accept
    // this format on input (especially important for decimal comma/dot).
    wxSetlocale(LC_ALL, "");
}

// ============================================================================
// other classes implementations
// ============================================================================

// ----------------------------------------------------------------------------
// wxAppTraits
// ----------------------------------------------------------------------------

#if wxUSE_THREADS
void wxMutexGuiEnterImpl();
void wxMutexGuiLeaveImpl();

void wxAppTraitsBase::MutexGuiEnter()
{
    wxMutexGuiEnterImpl();
}

void wxAppTraitsBase::MutexGuiLeave()
{
    wxMutexGuiLeaveImpl();
}

void WXDLLIMPEXP_BASE wxMutexGuiEnter()
{
    wxAppTraits * const traits = wxAppConsoleBase::GetTraitsIfExists();
    if ( traits )
        traits->MutexGuiEnter();
}

void WXDLLIMPEXP_BASE wxMutexGuiLeave()
{
    wxAppTraits * const traits = wxAppConsoleBase::GetTraitsIfExists();
    if ( traits )
        traits->MutexGuiLeave();
}
#endif // wxUSE_THREADS

// ============================================================================
// global functions implementation
// ============================================================================

void wxExit()
{
    if ( wxTheApp )
    {
        wxTheApp->Exit();
    }
    else
    {
        // what else can we do?
        exit(-1);
    }
}

void wxWakeUpIdle()
{
    if ( wxTheApp )
    {
        wxTheApp->WakeUpIdle();
    }
    //else: do nothing, what can we do?
}

void wxAbort()
{
#ifdef __WXWINCE__
    ExitThread(3);
#else
    abort();
#endif
}
