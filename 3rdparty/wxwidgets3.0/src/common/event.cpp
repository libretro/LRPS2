/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/event.cpp
// Purpose:     Event classes
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/event.h"
#include "wx/eventfilter.h"
#include "wx/evtloop.h"

#ifndef WX_PRECOMP
    #include "wx/list.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/module.h"
#endif

#include "wx/thread.h"

#if wxUSE_BASE
    #include "wx/scopedptr.h"

    wxDECLARE_SCOPED_PTR(wxEvent, wxEventPtr)
    wxDEFINE_SCOPED_PTR(wxEvent, wxEventPtr)
#endif // wxUSE_BASE

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

#if wxUSE_BASE
    IMPLEMENT_DYNAMIC_CLASS(wxEvtHandler, wxObject)
    IMPLEMENT_ABSTRACT_CLASS(wxEvent, wxObject)
    IMPLEMENT_DYNAMIC_CLASS(wxIdleEvent, wxEvent)
    IMPLEMENT_DYNAMIC_CLASS(wxThreadEvent, wxEvent)
#endif // wxUSE_BASE

#if wxUSE_BASE

const wxEventTable *wxEvtHandler::GetEventTable() const
    { return &wxEvtHandler::sm_eventTable; }

const wxEventTable wxEvtHandler::sm_eventTable =
    { (const wxEventTable *)NULL, &wxEvtHandler::sm_eventTableEntries[0] };

wxEventHashTable &wxEvtHandler::GetEventHashTable() const
    { return wxEvtHandler::sm_eventHashTable; }

wxEventHashTable wxEvtHandler::sm_eventHashTable(wxEvtHandler::sm_eventTable);

const wxEventTableEntry wxEvtHandler::sm_eventTableEntries[] =
    { wxDECLARE_EVENT_TABLE_TERMINATOR() };


// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

// common event types are defined here, other event types are defined by the
// components which use them

const wxEventType wxEVT_FIRST = 10000;
const wxEventType wxEVT_USER_FIRST = wxEVT_FIRST + 2000;
const wxEventType wxEVT_NULL = wxNewEventType();

wxDEFINE_EVENT( wxEVT_IDLE, wxIdleEvent );

// Thread and asynchronous call events
wxDEFINE_EVENT( wxEVT_THREAD, wxThreadEvent );
wxDEFINE_EVENT( wxEVT_ASYNC_METHOD_CALL, wxAsyncMethodCallEvent );

#endif // wxUSE_BASE

#if wxUSE_BASE

wxIdleMode wxIdleEvent::sm_idleMode = wxIDLE_PROCESS_ALL;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// event initialization
// ----------------------------------------------------------------------------

int wxNewEventType()
{
    // MT-FIXME
    static int s_lastUsedEventType = wxEVT_FIRST;

    return s_lastUsedEventType++;
}
// ----------------------------------------------------------------------------
// wxEventFunctor
// ----------------------------------------------------------------------------

wxEventFunctor::~wxEventFunctor()
{
}

// ----------------------------------------------------------------------------
// wxEvent
// ----------------------------------------------------------------------------

/*
 * General wxWidgets events, covering all interesting things that might happen
 * (button clicking, resizing, setting text in widgets, etc.).
 *
 * For each completely new event type, derive a new event class.
 *
 */

wxEvent::wxEvent(int theId, wxEventType commandType)
{
    m_eventType = commandType;
    m_eventObject = NULL;
    m_timeStamp = 0;
    m_id = theId;
    m_skipped = false;
    m_callbackUserData = NULL;
    m_handlerToProcessOnlyIn = NULL;
    m_isCommandEvent = false;
    m_propagationLevel = wxEVENT_PROPAGATE_NONE;
    m_propagatedFrom = NULL;
    m_wasProcessed = false;
    m_willBeProcessedAgain = false;
}

wxEvent::wxEvent(const wxEvent& src)
    : wxObject(src)
    , m_eventObject(src.m_eventObject)
    , m_eventType(src.m_eventType)
    , m_timeStamp(src.m_timeStamp)
    , m_id(src.m_id)
    , m_callbackUserData(src.m_callbackUserData)
    , m_handlerToProcessOnlyIn(NULL)
    , m_propagationLevel(src.m_propagationLevel)
    , m_propagatedFrom(NULL)
    , m_skipped(src.m_skipped)
    , m_isCommandEvent(src.m_isCommandEvent)
    , m_wasProcessed(false)
    , m_willBeProcessedAgain(false)
{
}

wxEvent& wxEvent::operator=(const wxEvent& src)
{
    wxObject::operator=(src);

    m_eventObject = src.m_eventObject;
    m_eventType = src.m_eventType;
    m_timeStamp = src.m_timeStamp;
    m_id = src.m_id;
    m_callbackUserData = src.m_callbackUserData;
    m_handlerToProcessOnlyIn = NULL;
    m_propagationLevel = src.m_propagationLevel;
    m_propagatedFrom = NULL;
    m_skipped = src.m_skipped;
    m_isCommandEvent = src.m_isCommandEvent;

    // don't change m_wasProcessed

    // While the original again could be passed to another handler, this one
    // isn't going to be processed anywhere else by default.
    m_willBeProcessedAgain = false;

    return *this;
}

#endif // wxUSE_BASE

#if wxUSE_BASE

// ----------------------------------------------------------------------------
// wxEventHashTable
// ----------------------------------------------------------------------------

static const int EVENT_TYPE_TABLE_INIT_SIZE = 31; // Not too big not too small...

wxEventHashTable* wxEventHashTable::sm_first = NULL;

wxEventHashTable::wxEventHashTable(const wxEventTable &table)
                : m_table(table),
                  m_rebuildHash(true)
{
    AllocEventTypeTable(EVENT_TYPE_TABLE_INIT_SIZE);

    m_next = sm_first;
    if (m_next)
        m_next->m_previous = this;
    sm_first = this;
}

wxEventHashTable::~wxEventHashTable()
{
    if (m_next)
        m_next->m_previous = m_previous;
    if (m_previous)
        m_previous->m_next = m_next;
    if (sm_first == this)
        sm_first = m_next;

    Clear();
}

void wxEventHashTable::Clear()
{
    for ( size_t i = 0; i < m_size; i++ )
    {
        EventTypeTablePointer  eTTnode = m_eventTypeTable[i];
        delete eTTnode;
    }

    wxDELETEA(m_eventTypeTable);

    m_size = 0;
}

bool wxEventHashTable::HandleEvent(wxEvent &event, wxEvtHandler *self)
{
    if (m_rebuildHash)
    {
        InitHashTable();
        m_rebuildHash = false;
    }

    if (!m_eventTypeTable)
        return false;

    // Find all entries for the given event type.
    wxEventType eventType = event.GetEventType();
    const EventTypeTablePointer eTTnode = m_eventTypeTable[eventType % m_size];
    if (eTTnode && eTTnode->eventType == eventType)
    {
        // Now start the search for an event handler
        // that can handle an event with the given ID.
        const wxEventTableEntryPointerArray&
            eventEntryTable = eTTnode->eventEntryTable;

        const size_t count = eventEntryTable.GetCount();
        for (size_t n = 0; n < count; n++)
        {
            const wxEventTableEntry& entry = *eventEntryTable[n];
            if ( wxEvtHandler::ProcessEventIfMatchesId(entry, self, event) )
                return true;
        }
    }

    return false;
}

void wxEventHashTable::InitHashTable()
{
    // Loop over the event tables and all its base tables.
    const wxEventTable *table = &m_table;
    while (table)
    {
        // Retrieve all valid event handler entries
        const wxEventTableEntry *entry = table->entries;
        while (entry->m_fn != 0)
        {
            // Add the event entry in the Hash.
            AddEntry(*entry);

            entry++;
        }

        table = table->baseTable;
    }

    // Let's free some memory.
    size_t i;
    for(i = 0; i < m_size; i++)
    {
        EventTypeTablePointer  eTTnode = m_eventTypeTable[i];
        if (eTTnode)
        {
            eTTnode->eventEntryTable.Shrink();
        }
    }
}

void wxEventHashTable::AddEntry(const wxEventTableEntry &entry)
{
    // This might happen 'accidentally' as the app is exiting
    if (!m_eventTypeTable)
        return;

    EventTypeTablePointer *peTTnode = &m_eventTypeTable[entry.m_eventType % m_size];
    EventTypeTablePointer  eTTnode = *peTTnode;

    if (eTTnode)
    {
        if (eTTnode->eventType != entry.m_eventType)
        {
            // Resize the table!
            GrowEventTypeTable();
            // Try again to add it.
            AddEntry(entry);
            return;
        }
    }
    else
    {
        eTTnode = new EventTypeTable;
        eTTnode->eventType = entry.m_eventType;
        *peTTnode = eTTnode;
    }

    // Fill all hash entries between entry.m_id and entry.m_lastId...
    eTTnode->eventEntryTable.Add(&entry);
}

void wxEventHashTable::AllocEventTypeTable(size_t size)
{
    m_eventTypeTable = new EventTypeTablePointer[size];
    memset((void *)m_eventTypeTable, 0, sizeof(EventTypeTablePointer)*size);
    m_size = size;
}

void wxEventHashTable::GrowEventTypeTable()
{
    size_t oldSize = m_size;
    EventTypeTablePointer *oldEventTypeTable = m_eventTypeTable;

    // TODO: Search the most optimal grow sequence
    AllocEventTypeTable(/* GetNextPrime(oldSize) */oldSize*2+1);

    for ( size_t i = 0; i < oldSize; /* */ )
    {
        EventTypeTablePointer  eTToldNode = oldEventTypeTable[i];
        if (eTToldNode)
        {
            EventTypeTablePointer *peTTnode = &m_eventTypeTable[eTToldNode->eventType % m_size];
            EventTypeTablePointer  eTTnode = *peTTnode;

            // Check for collision, we don't want any.
            if (eTTnode)
            {
                GrowEventTypeTable();
                continue; // Don't increment the counter,
                          // as we still need to add this element.
            }
            else
            {
                // Get the old value and put it in the new table.
                *peTTnode = oldEventTypeTable[i];
            }
        }

        i++;
    }

    delete[] oldEventTypeTable;
}

// ----------------------------------------------------------------------------
// wxEvtHandler
// ----------------------------------------------------------------------------

wxEvtHandler::wxEvtHandler()
{
    m_nextHandler = NULL;
    m_previousHandler = NULL;
    m_enabled = true;
    m_pendingEvents = NULL;
}

wxEvtHandler::~wxEvtHandler()
{
    Unlink();

    // Remove us from the list of the pending events if necessary.
    if (wxTheApp)
        wxTheApp->RemovePendingEventHandler(this);

    DeletePendingEvents();
}

void wxEvtHandler::Unlink()
{
    // this event handler must take itself out of the chain of handlers:

    if (m_previousHandler)
        m_previousHandler->SetNextHandler(m_nextHandler);

    if (m_nextHandler)
        m_nextHandler->SetPreviousHandler(m_previousHandler);

    m_nextHandler = NULL;
    m_previousHandler = NULL;
}

wxEventFilter* wxEvtHandler::ms_filterList = NULL;

/* static */ void wxEvtHandler::AddFilter(wxEventFilter* filter)
{
    wxCHECK_RET( filter, "NULL filter" );

    filter->m_next = ms_filterList;
    ms_filterList = filter;
}

/* static */ void wxEvtHandler::RemoveFilter(wxEventFilter* filter)
{
    wxEventFilter* prev = NULL;
    for ( wxEventFilter* f = ms_filterList; f; f = f->m_next )
    {
        if ( f == filter )
        {
            // Set the previous list element or the list head to the next
            // element.
            if ( prev )
                prev->m_next = f->m_next;
            else
                ms_filterList = f->m_next;

            // Also reset the next pointer in the filter itself just to avoid
            // having possibly dangling pointers, even though it's not strictly
            // necessary.
            f->m_next = NULL;

            // Skip the assert below.
            return;
        }

        prev = f;
    }

    wxFAIL_MSG( "Filter not found" );
}

#if wxUSE_THREADS

bool wxEvtHandler::ProcessThreadEvent(const wxEvent& event)
{
    // check that we are really in a child thread
    wxASSERT_MSG( !wxThread::IsMain(),
                  wxT("use ProcessEvent() in main thread") );

    AddPendingEvent(event);

    return true;
}

#endif // wxUSE_THREADS

void wxEvtHandler::QueueEvent(wxEvent *event)
{
    wxCHECK_RET( event, "NULL event can't be posted" );

    if (!wxTheApp)
    {
        // anyway delete the given event to avoid memory leaks
        delete event;

        return;
    }

    // 1) Add this event to our list of pending events
    wxENTER_CRIT_SECT( m_pendingEventsLock );

    if ( !m_pendingEvents )
        m_pendingEvents = new wxList;

    m_pendingEvents->Append(event);

    // 2) Add this event handler to list of event handlers that
    //    have pending events.

    wxTheApp->AppendPendingEventHandler(this);

    // only release m_pendingEventsLock now because otherwise there is a race
    // condition as described in the ticket #9093: we could process the event
    // just added to m_pendingEvents in our ProcessPendingEvents() below before
    // we had time to append this pointer to wxHandlersWithPendingEvents list; thus
    // breaking the invariant that a handler should be in the list iff it has
    // any pending events to process
    wxLEAVE_CRIT_SECT( m_pendingEventsLock );

    // 3) Inform the system that new pending events are somewhere,
    //    and that these should be processed in idle time.
    wxWakeUpIdle();
}

void wxEvtHandler::DeletePendingEvents()
{
    if (m_pendingEvents)
        m_pendingEvents->DeleteContents(true);
    wxDELETE(m_pendingEvents);
}

void wxEvtHandler::ProcessPendingEvents()
{
    if (!wxTheApp)
        return;

    // we need to process only a single pending event in this call because
    // each call to ProcessEvent() could result in the destruction of this
    // same event handler (see the comment at the end of this function)

    wxENTER_CRIT_SECT( m_pendingEventsLock );

    // this method is only called by wxApp if this handler does have
    // pending events
    wxCHECK_RET( m_pendingEvents && !m_pendingEvents->IsEmpty(),
                 "should have pending events if called" );

    wxList::compatibility_iterator node = m_pendingEvents->GetFirst();
    wxEvent* pEvent = static_cast<wxEvent *>(node->GetData());

    wxEventPtr event(pEvent);

    // it's important we remove event from list before processing it, else a
    // nested event loop, for example from a modal dialog, might process the
    // same event again.
    m_pendingEvents->Erase(node);

    if ( m_pendingEvents->IsEmpty() )
    {
        // if there are no more pending events left, we don't need to
        // stay in this list
        wxTheApp->RemovePendingEventHandler(this);
    }

    wxLEAVE_CRIT_SECT( m_pendingEventsLock );

    ProcessEvent(*event);

    // careful: this object could have been deleted by the event handler
    // executed by the above ProcessEvent() call, so we can't access any fields
    // of this object any more
}

/* static */
bool wxEvtHandler::ProcessEventIfMatchesId(const wxEventTableEntryBase& entry,
                                           wxEvtHandler *handler,
                                           wxEvent& event)
{
    int tableId1 = entry.m_id,
        tableId2 = entry.m_lastId;

    // match only if the event type is the same and the id is either -1 in
    // the event table (meaning "any") or the event id matches the id
    // specified in the event table either exactly or by falling into
    // range between first and last
    if ((tableId1 == wxID_ANY) ||
        (tableId2 == wxID_ANY && tableId1 == event.GetId()) ||
        (tableId2 != wxID_ANY &&
         (event.GetId() >= tableId1 && event.GetId() <= tableId2)))
    {
        event.Skip(false);
        event.m_callbackUserData = entry.m_callbackUserData;

        {
            (*entry.m_fn)(handler, event);
        }

        if (!event.GetSkipped())
            return true;
    }

    return false;
}

bool wxEvtHandler::DoTryApp(wxEvent& event)
{
    if ( wxTheApp && (this != wxTheApp) )
    {
        // Special case: don't pass wxEVT_IDLE to wxApp, since it'll always
        // swallow it. wxEVT_IDLE is sent explicitly to wxApp so it will be
        // processed appropriately via SearchEventTable.
        if ( event.GetEventType() != wxEVT_IDLE )
        {
            if ( wxTheApp->ProcessEvent(event) )
                return true;
        }
    }

    return false;
}

bool wxEvtHandler::TryBefore(wxEvent& event)
{
#if WXWIN_COMPATIBILITY_2_8
    // call the old virtual function to keep the code overriding it working
    return TryValidator(event);
#else
    wxUnusedVar(event);
    return false;
#endif
}

bool wxEvtHandler::TryAfter(wxEvent& event)
{
    // We only want to pass the window to the application object once even if
    // there are several chained handlers. Ensure that this is what happens by
    // only calling DoTryApp() if there is no next handler (which would do it).
    //
    // Notice that, unlike simply calling TryAfter() on the last handler in the
    // chain only from ProcessEvent(), this also works with wxWindow object in
    // the middle of the chain: its overridden TryAfter() will still be called
    // and propagate the event upwards the window hierarchy even if it's not
    // the last one in the chain (which, admittedly, shouldn't happen often).
    if ( GetNextHandler() )
        return GetNextHandler()->TryAfter(event);

    // If this event is going to be processed in another handler next, don't
    // pass it to wxTheApp now, it will be done from TryAfter() of this other
    // handler.
    if ( event.WillBeProcessedAgain() )
        return false;

#if WXWIN_COMPATIBILITY_2_8
    // as above, call the old virtual function for compatibility
    return TryParent(event);
#else
    return DoTryApp(event);
#endif
}

bool wxEvtHandler::ProcessEvent(wxEvent& event)
{
    // The very first thing we do is to allow any registered filters to hook
    // into event processing in order to globally pre-process all events.
    //
    // Note that we should only do it if we're the first event handler called
    // to avoid calling FilterEvent() multiple times as the event goes through
    // the event handler chain and possibly upwards the window hierarchy.
    if ( !event.WasProcessed() )
    {
        for ( wxEventFilter* f = ms_filterList; f; f = f->m_next )
        {
            int rc = f->FilterEvent(event);
            if ( rc != wxEventFilter::Event_Skip )
            {
                wxASSERT_MSG( rc == wxEventFilter::Event_Ignore ||
                                rc == wxEventFilter::Event_Processed,
                              "unexpected FilterEvent() return value" );

                return rc != wxEventFilter::Event_Ignore;
            }
            //else: proceed normally
        }
    }

    // Short circuit the event processing logic if we're requested to process
    // this event in this handler only, see DoTryChain() for more details.
    if ( event.ShouldProcessOnlyIn(this) )
        return TryBeforeAndHere(event);


    // Try to process the event in this handler itself.
    if ( ProcessEventLocally(event) )
    {
        // It is possible that DoTryChain() called from ProcessEventLocally()
        // returned true but the event was not really processed: this happens
        // if a custom handler ignores the request to process the event in this
        // handler only and in this case we should skip the post processing
        // done in TryAfter() but still return the correct value ourselves to
        // indicate whether we did or did not find a handler for this event.
        return !event.GetSkipped();
    }

    // If we still didn't find a handler, propagate the event upwards the
    // window chain and/or to the application object.
    if ( TryAfter(event) )
        return true;


    // No handler found anywhere, bail out.
    return false;
}

bool wxEvtHandler::ProcessEventLocally(wxEvent& event)
{
    // Try the hooks which should be called before our own handlers and this
    // handler itself first. Notice that we should not call ProcessEvent() on
    // this one as we're already called from it, which explains why we do it
    // here and not in DoTryChain()
    return TryBeforeAndHere(event) || DoTryChain(event);
}

bool wxEvtHandler::DoTryChain(wxEvent& event)
{
    for ( wxEvtHandler *h = GetNextHandler(); h; h = h->GetNextHandler() )
    {
        // We need to process this event at the level of this handler only
        // right now, the pre-/post-processing was either already done by
        // ProcessEvent() from which we were called or will be done by it when
        // we return.
        //
        // However we must call ProcessEvent() and not TryHereOnly() because the
        // existing code (including some in wxWidgets itself) expects the
        // overridden ProcessEvent() in its custom event handlers pushed on a
        // window to be called.
        //
        // So we must call ProcessEvent() but it must not do what it usually
        // does. To resolve this paradox we set up a special flag inside the
        // object itself to let ProcessEvent() know that it shouldn't do any
        // pre/post-processing for this event if it gets it. Note that this
        // only applies to this handler, if the event is passed to another one
        // by explicitly calling its ProcessEvent(), pre/post-processing should
        // be done as usual.
        //
        // Final complication is that if the implementation of ProcessEvent()
        // called wxEvent::DidntHonourProcessOnlyIn() (as the gross hack that
        // is wxScrollHelperEvtHandler::ProcessEvent() does) and ignored our
        // request to process event in this handler only, we have to compensate
        // for it by not processing the event further because this was already
        // done by that rogue event handler.
        wxEventProcessInHandlerOnly processInHandlerOnly(event, h);
        if ( h->ProcessEvent(event) )
        {
            // Make sure "skipped" flag is not set as the event was really
            // processed in this case. Normally it shouldn't be set anyhow but
            // make sure just in case the user code does something strange.
            event.Skip(false);

            return true;
        }

        if ( !event.ShouldProcessOnlyIn(h) )
        {
            // Still return true to indicate that no further processing should
            // be undertaken but ensure that "skipped" flag is set so that the
            // caller knows that the event was not really processed.
            event.Skip();

            return true;
        }
    }

    return false;
}

bool wxEvtHandler::TryHereOnly(wxEvent& event)
{
    // If the event handler is disabled it doesn't process any events
    if ( GetEvtHandlerEnabled() )
    {
	    // Then static per-class event tables
	    if ( GetEventHashTable().HandleEvent(event, this) )
		    return true;
    }

    // We don't have a handler for this event.
    return false;
}

void wxEvtHandler::OnSinkDestroyed( wxEvtHandler *sink )
{
}

#endif // wxUSE_BASE
