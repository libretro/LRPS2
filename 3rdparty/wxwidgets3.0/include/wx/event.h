/////////////////////////////////////////////////////////////////////////////
// Name:        wx/event.h
// Purpose:     Event classes
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EVENT_H_
#define _WX_EVENT_H_

#include "wx/defs.h"
#include "wx/cpp.h"
#include "wx/object.h"

#include "wx/dynarray.h"
#include "wx/thread.h"
#include "wx/tracker.h"
#include "wx/typeinfo.h"

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_BASE wxList;
class WXDLLIMPEXP_FWD_BASE wxEvent;
class WXDLLIMPEXP_FWD_BASE wxEventFilter;

// We operate with pointer to members of wxEvtHandler (such functions are used
// as event handlers in the event tables or as arguments to Connect()) but by
// default MSVC uses a restricted (but more efficient) representation of
// pointers to members which can't deal with multiple base classes. To avoid
// mysterious (as the compiler is not good enough to detect this and give a
// sensible error message) errors in the user code as soon as it defines
// classes inheriting from both wxEvtHandler (possibly indirectly, e.g. via
// wxWindow) and something else (including our own wxTrackable but not limited
// to it), we use the special MSVC keyword telling the compiler to use a more
// general pointer to member representation for the classes inheriting from
// wxEvtHandler.
#ifdef __VISUALC__
    #define wxMSVC_FWD_MULTIPLE_BASES __multiple_inheritance
#else
    #define wxMSVC_FWD_MULTIPLE_BASES
#endif

class WXDLLIMPEXP_FWD_BASE wxMSVC_FWD_MULTIPLE_BASES wxEvtHandler;

// ----------------------------------------------------------------------------
// Event types
// ----------------------------------------------------------------------------

typedef int wxEventType;

#define wxEVT_ANY           ((wxEventType)-1)

// this is used to make the event table entry type safe, so that for an event
// handler only a function with proper parameter list can be given. See also
// the wxEVENT_HANDLER_CAST-macro.
#define wxStaticCastEvent(type, val) static_cast<type>(val)

#define wxDECLARE_EVENT_TABLE_ENTRY(type, winid, idLast, fn, obj) \
    wxEventTableEntry(type, winid, idLast, wxNewEventTableFunctor(type, fn), obj)

#define wxDECLARE_EVENT_TABLE_TERMINATOR() \
    wxEventTableEntry(wxEVT_NULL, 0, 0, 0, 0)

// generate a new unique event type
extern WXDLLIMPEXP_BASE wxEventType wxNewEventType();

// define macros to create new event types:
    // the macros are the same ones as above but defined differently as we only
    // use the integer event type values to identify events in this case

    #define wxDEFINE_EVENT( name, type ) \
        const wxEventType name( wxNewEventType() )

    #define wxDECLARE_EXPORTED_EVENT( expdecl, name, type ) \
        extern const expdecl wxEventType name
    #define wxDECLARE_EVENT( name, type ) \
        wxDECLARE_EXPORTED_EVENT( wxEMPTY_PARAMETER_VALUE, name, type )

    #define wxDEFINE_EVENT_ALIAS( name, type, value ) \
        const wxEventType name = value
    #define wxDECLARE_EXPORTED_EVENT_ALIAS( expdecl, name, type ) \
        extern const expdecl wxEventType name

// Try to cast the given event handler to the correct handler type:

#define wxEVENT_HANDLER_CAST( functype, func ) \
    ( wxObjectEventFunction )( wxEventFunction )wxStaticCastEvent( functype, &func )


// These are needed for the functor definitions
typedef void (wxEvtHandler::*wxEventFunction)(wxEvent&);

// We had some trouble (specifically with eVC for ARM WinCE build) with using
// wxEventFunction in the past so we had introduced wxObjectEventFunction which
// used to be a typedef for a member of wxObject and not wxEvtHandler to work
// around this but as eVC is not really supported any longer we now only keep
// this for backwards compatibility and, despite its name, this is a typedef
// for wxEvtHandler member now -- but if we have the same problem with another
// compiler we can restore its old definition for it.
typedef wxEventFunction wxObjectEventFunction;

// The event functor which is stored in the static and dynamic event tables:
class WXDLLIMPEXP_BASE wxEventFunctor
{
public:
    virtual ~wxEventFunctor();

    // Invoke the actual event handler:
    virtual void operator()(wxEvtHandler *, wxEvent&) = 0;

private:
    WX_DECLARE_ABSTRACT_TYPEINFO(wxEventFunctor)
};

// A plain method functor for the untyped legacy event types:
class WXDLLIMPEXP_BASE wxObjectEventFunctor : public wxEventFunctor
{
public:
    wxObjectEventFunctor(wxObjectEventFunction method, wxEvtHandler *handler)
        : m_handler( handler ), m_method( method )
        { }

    virtual void operator()(wxEvtHandler *handler, wxEvent& event);

private:
    wxEvtHandler *m_handler;
    wxEventFunction m_method;

    // Provide a dummy default ctor for type info purposes
    wxObjectEventFunctor() { }

    WX_DECLARE_TYPEINFO_INLINE(wxObjectEventFunctor)
};

// Create a functor for the legacy events: used by Connect()
inline wxObjectEventFunctor *
wxNewEventFunctor(const wxEventType& WXUNUSED(evtType),
                  wxObjectEventFunction method,
                  wxEvtHandler *handler)
{
    return new wxObjectEventFunctor(method, handler);
}

// This version is used by wxDECLARE_EVENT_TABLE_ENTRY()
inline wxObjectEventFunctor *
wxNewEventTableFunctor(const wxEventType& WXUNUSED(evtType),
                       wxObjectEventFunction method)
{
    return new wxObjectEventFunctor(method, NULL);
}

inline wxObjectEventFunctor
wxMakeEventFunctor(const wxEventType& WXUNUSED(evtType),
                        wxObjectEventFunction method,
                        wxEvtHandler *handler)
{
    return wxObjectEventFunctor(method, handler);
}

// many, but not all, standard event types

    // some generic events
extern WXDLLIMPEXP_BASE const wxEventType wxEVT_NULL;
extern WXDLLIMPEXP_BASE const wxEventType wxEVT_FIRST;
extern WXDLLIMPEXP_BASE const wxEventType wxEVT_USER_FIRST;

    // Need events declared to do this
class WXDLLIMPEXP_FWD_BASE wxIdleEvent;
class WXDLLIMPEXP_FWD_BASE wxThreadEvent;
class WXDLLIMPEXP_FWD_BASE wxAsyncMethodCallEvent;
class WXDLLIMPEXP_FWD_CORE wxCommandEvent;
class WXDLLIMPEXP_FWD_CORE wxMouseEvent;
class WXDLLIMPEXP_FWD_CORE wxFocusEvent;
class WXDLLIMPEXP_FWD_CORE wxChildFocusEvent;
class WXDLLIMPEXP_FWD_CORE wxKeyEvent;
class WXDLLIMPEXP_FWD_CORE wxNavigationKeyEvent;
class WXDLLIMPEXP_FWD_CORE wxSetCursorEvent;
class WXDLLIMPEXP_FWD_CORE wxScrollEvent;
class WXDLLIMPEXP_FWD_CORE wxSpinEvent;
class WXDLLIMPEXP_FWD_CORE wxScrollWinEvent;
class WXDLLIMPEXP_FWD_CORE wxSizeEvent;
class WXDLLIMPEXP_FWD_CORE wxMoveEvent;
class WXDLLIMPEXP_FWD_CORE wxCloseEvent;
class WXDLLIMPEXP_FWD_CORE wxActivateEvent;
class WXDLLIMPEXP_FWD_CORE wxWindowCreateEvent;
class WXDLLIMPEXP_FWD_CORE wxWindowDestroyEvent;
class WXDLLIMPEXP_FWD_CORE wxShowEvent;
class WXDLLIMPEXP_FWD_CORE wxMaximizeEvent;
class WXDLLIMPEXP_FWD_CORE wxMouseCaptureChangedEvent;
class WXDLLIMPEXP_FWD_CORE wxMouseCaptureLostEvent;
class WXDLLIMPEXP_FWD_CORE wxPaintEvent;
class WXDLLIMPEXP_FWD_CORE wxEraseEvent;
class WXDLLIMPEXP_FWD_CORE wxNcPaintEvent;
class WXDLLIMPEXP_FWD_CORE wxMenuEvent;
class WXDLLIMPEXP_FWD_CORE wxContextMenuEvent;
class WXDLLIMPEXP_FWD_CORE wxSysColourChangedEvent;
class WXDLLIMPEXP_FWD_CORE wxDisplayChangedEvent;
class WXDLLIMPEXP_FWD_CORE wxQueryNewPaletteEvent;
class WXDLLIMPEXP_FWD_CORE wxJoystickEvent;
class WXDLLIMPEXP_FWD_CORE wxDropFilesEvent;
class WXDLLIMPEXP_FWD_CORE wxInitDialogEvent;
class WXDLLIMPEXP_FWD_CORE wxUpdateUIEvent;
class WXDLLIMPEXP_FWD_CORE wxClipboardTextEvent;
class WXDLLIMPEXP_FWD_CORE wxHelpEvent;


    // Command events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_BUTTON, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHECKBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHOICE, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LISTBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LISTBOX_DCLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHECKLISTBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MENU, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SLIDER, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RADIOBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RADIOBUTTON, wxCommandEvent);

// wxEVT_SCROLLBAR is deprecated, use wxEVT_SCROLL... events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLBAR, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_VLBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMBOBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TOOL_RCLICKED, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TOOL_DROPDOWN, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TOOL_ENTER, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMBOBOX_DROPDOWN, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMBOBOX_CLOSEUP, wxCommandEvent);

    // Thread and asynchronous method call events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_BASE, wxEVT_THREAD, wxThreadEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_BASE, wxEVT_ASYNC_METHOD_CALL, wxAsyncMethodCallEvent);

    // Mouse event types
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LEFT_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LEFT_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MIDDLE_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MIDDLE_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RIGHT_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RIGHT_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOTION, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ENTER_WINDOW, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LEAVE_WINDOW, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LEFT_DCLICK, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MIDDLE_DCLICK, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RIGHT_DCLICK, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SET_FOCUS, wxFocusEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_KILL_FOCUS, wxFocusEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHILD_FOCUS, wxChildFocusEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOUSEWHEEL, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX1_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX1_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX1_DCLICK, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX2_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX2_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX2_DCLICK, wxMouseEvent);

    // Character input event type
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHAR, wxKeyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHAR_HOOK, wxKeyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_NAVIGATION_KEY, wxNavigationKeyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_KEY_DOWN, wxKeyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_KEY_UP, wxKeyEvent);
// This is a private event used by wxMSW code only and subject to change or
// disappear in the future. Don't use.
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AFTER_CHAR, wxKeyEvent);

    // Set cursor event
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SET_CURSOR, wxSetCursorEvent);

    // wxScrollBar and wxSlider event identifiers
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_TOP, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_BOTTOM, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_LINEUP, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_LINEDOWN, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_PAGEUP, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_PAGEDOWN, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_THUMBTRACK, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_THUMBRELEASE, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_CHANGED, wxScrollEvent);

    // Scroll events from wxWindow
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_TOP, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_BOTTOM, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_LINEUP, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_LINEDOWN, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_PAGEUP, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_PAGEDOWN, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_THUMBTRACK, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_THUMBRELEASE, wxScrollWinEvent);

    // System events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SIZE, wxSizeEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOVE, wxMoveEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CLOSE_WINDOW, wxCloseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_END_SESSION, wxCloseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_QUERY_END_SESSION, wxCloseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ACTIVATE_APP, wxActivateEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ACTIVATE, wxActivateEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CREATE, wxWindowCreateEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_DESTROY, wxWindowDestroyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SHOW, wxShowEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MAXIMIZE, wxMaximizeEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOUSE_CAPTURE_CHANGED, wxMouseCaptureChangedEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOUSE_CAPTURE_LOST, wxMouseCaptureLostEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_PAINT, wxPaintEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ERASE_BACKGROUND, wxEraseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_NC_PAINT, wxNcPaintEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MENU_OPEN, wxMenuEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MENU_CLOSE, wxMenuEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MENU_HIGHLIGHT, wxMenuEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CONTEXT_MENU, wxContextMenuEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SYS_COLOUR_CHANGED, wxSysColourChangedEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_DISPLAY_CHANGED, wxDisplayChangedEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_QUERY_NEW_PALETTE, wxQueryNewPaletteEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_JOY_BUTTON_DOWN, wxJoystickEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_JOY_BUTTON_UP, wxJoystickEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_JOY_MOVE, wxJoystickEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_JOY_ZMOVE, wxJoystickEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_DROP_FILES, wxDropFilesEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_INIT_DIALOG, wxInitDialogEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_BASE, wxEVT_IDLE, wxIdleEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_UPDATE_UI, wxUpdateUIEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SIZING, wxSizeEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOVING, wxMoveEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOVE_START, wxMoveEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOVE_END, wxMoveEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_HIBERNATE, wxActivateEvent);

    // Clipboard events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_COPY, wxClipboardTextEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_CUT, wxClipboardTextEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_PASTE, wxClipboardTextEvent);

    // Generic command events
    // Note: a click is a higher-level event than button down/up
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_LEFT_CLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_LEFT_DCLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_RIGHT_CLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_RIGHT_DCLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_SET_FOCUS, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_KILL_FOCUS, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_ENTER, wxCommandEvent);

    // Help events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_HELP, wxHelpEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_DETAILED_HELP, wxHelpEvent);

// these 2 events are the same
#define wxEVT_TOOL wxEVT_MENU

// ----------------------------------------------------------------------------
// Compatibility
// ----------------------------------------------------------------------------

// this event is also used by wxComboBox and wxSpinCtrl which don't include
// wx/textctrl.h in all ports [yet], so declare it here as well
//
// still, any new code using it should include wx/textctrl.h explicitly
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT, wxCommandEvent);


// ----------------------------------------------------------------------------
// wxEvent(-derived) classes
// ----------------------------------------------------------------------------

// the predefined constants for the number of times we propagate event
// upwards window child-parent chain
enum wxEventPropagation
{
    // don't propagate it at all
    wxEVENT_PROPAGATE_NONE = 0,

    // propagate it until it is processed
    wxEVENT_PROPAGATE_MAX = INT_MAX
};

/*
 * wxWidgets events, covering all interesting things that might happen
 * (button clicking, resizing, setting text in widgets, etc.).
 *
 * For each completely new event type, derive a new event class.
 * An event CLASS represents a C++ class defining a range of similar event TYPES;
 * examples are canvas events, panel item command events.
 * An event TYPE is a unique identifier for a particular system event,
 * such as a button press or a listbox deselection.
 *
 */

class WXDLLIMPEXP_BASE wxEvent : public wxObject
{
public:
    wxEvent(int winid = 0, wxEventType commandType = wxEVT_NULL );

    wxEventType GetEventType() const { return m_eventType; }

    wxObject *GetEventObject() const { return m_eventObject; }
    void SetEventObject(wxObject *obj) { m_eventObject = obj; }

    int GetId() const { return m_id; }
    void SetId(int Id) { m_id = Id; }

    // Can instruct event processor that we wish to ignore this event
    // (treat as if the event table entry had not been found): this must be done
    // to allow the event processing by the base classes (calling event.Skip()
    // is the analog of calling the base class version of a virtual function)
    void Skip(bool skip = true) { m_skipped = skip; }
    bool GetSkipped() const { return m_skipped; }

    // This function is used to create a copy of the event polymorphically and
    // all derived classes must implement it because otherwise wxPostEvent()
    // for them wouldn't work (it needs to do a copy of the event)
    virtual wxEvent *Clone() const = 0;

    // Stop an event from propagating to its parent window, returns the old
    // propagation level value
    int StopPropagation()
    {
        int propagationLevel = m_propagationLevel;
        m_propagationLevel = wxEVENT_PROPAGATE_NONE;
        return propagationLevel;
    }

    // Resume the event propagation by restoring the propagation level
    // (returned by StopPropagation())
    void ResumePropagation(int propagationLevel)
    {
        m_propagationLevel = propagationLevel;
    }

    // This is for internal use only and is only called by
    // wxEvtHandler::ProcessEvent() to check whether it's the first time this
    // event is being processed
    bool WasProcessed()
    {
        if ( m_wasProcessed )
            return true;

        m_wasProcessed = true;

        return false;
    }

    // This is also used only internally by ProcessEvent() to check if it
    // should process the event normally or only restrict the search for the
    // event handler to this object itself.
    bool ShouldProcessOnlyIn(wxEvtHandler *h) const
    {
        return h == m_handlerToProcessOnlyIn;
    }

protected:
    wxObject*         m_eventObject;
    wxEventType       m_eventType;
    int               m_id;

public:
    // m_callbackUserData is for internal usage only
    wxObject*         m_callbackUserData;

private:
    // If this handler
    wxEvtHandler *m_handlerToProcessOnlyIn;

protected:
    // the propagation level: while it is positive, we propagate the event to
    // the parent window (if any)
    int               m_propagationLevel;

    // The object that the event is being propagated from, initially NULL and
    // only set by wxPropagateOnce.
    wxEvtHandler*     m_propagatedFrom;

    bool              m_skipped;

    // initially false but becomes true as soon as WasProcessed() is called for
    // the first time, as this is done only by ProcessEvent() it explains the
    // variable name: it becomes true after ProcessEvent() was called at least
    // once for this event
    bool m_wasProcessed;

protected:
    wxEvent(const wxEvent&);            // for implementing Clone()
    wxEvent& operator=(const wxEvent&); // for derived classes operator=()

private:
    // It needs to access our m_propagationLevel and m_propagatedFrom fields.
    friend class WXDLLIMPEXP_FWD_BASE wxPropagateOnce;

    // and this one needs to access our m_handlerToProcessOnlyIn
    friend class WXDLLIMPEXP_FWD_BASE wxEventProcessInHandlerOnly;


    DECLARE_ABSTRACT_CLASS(wxEvent)
};

/*
 * Helper class to temporarily change an event not to propagate.
 */
class WXDLLIMPEXP_BASE wxPropagationDisabler
{
public:
    wxPropagationDisabler(wxEvent& event) : m_event(event)
    {
        m_propagationLevelOld = m_event.StopPropagation();
    }

    ~wxPropagationDisabler()
    {
        m_event.ResumePropagation(m_propagationLevelOld);
    }

private:
    wxEvent& m_event;
    int m_propagationLevelOld;

    wxDECLARE_NO_COPY_CLASS(wxPropagationDisabler);
};

/*
 * Helper used to indicate that an event is propagated upwards the window
 * hierarchy by the given window.
 */
class WXDLLIMPEXP_BASE wxPropagateOnce
{
public:
    // The handler argument should normally be non-NULL to allow the parent
    // event handler to know that it's being used to process an event coming
    // from the child, it's only NULL by default for backwards compatibility.
    wxPropagateOnce(wxEvent& event, wxEvtHandler* handler = NULL)
        : m_event(event),
          m_propagatedFromOld(event.m_propagatedFrom)
    {
        m_event.m_propagationLevel--;
        m_event.m_propagatedFrom = handler;
    }

    ~wxPropagateOnce()
    {
        m_event.m_propagatedFrom = m_propagatedFromOld;
        m_event.m_propagationLevel++;
    }

private:
    wxEvent& m_event;
    wxEvtHandler* const m_propagatedFromOld;

    wxDECLARE_NO_COPY_CLASS(wxPropagateOnce);
};

// A helper object used to temporarily make wxEvent::ShouldProcessOnlyIn()
// return true for the handler passed to its ctor.
class wxEventProcessInHandlerOnly
{
public:
    wxEventProcessInHandlerOnly(wxEvent& event, wxEvtHandler *handler)
        : m_event(event),
          m_handlerToProcessOnlyInOld(event.m_handlerToProcessOnlyIn)
    {
        m_event.m_handlerToProcessOnlyIn = handler;
    }

    ~wxEventProcessInHandlerOnly()
    {
        m_event.m_handlerToProcessOnlyIn = m_handlerToProcessOnlyInOld;
    }

private:
    wxEvent& m_event;
    wxEvtHandler * const m_handlerToProcessOnlyInOld;

    wxDECLARE_NO_COPY_CLASS(wxEventProcessInHandlerOnly);
};


class WXDLLIMPEXP_BASE wxEventBasicPayloadMixin
{
public:
    wxEventBasicPayloadMixin()
        : m_commandInt(0),
          m_extraLong(0)
    {
    }

    void SetString(const wxString& s) { m_cmdString = s; }
    const wxString& GetString() const { return m_cmdString; }

    void SetInt(int i) { m_commandInt = i; }
    int GetInt() const { return m_commandInt; }

    void SetExtraLong(long extraLong) { m_extraLong = extraLong; }
    long GetExtraLong() const { return m_extraLong; }

protected:
    // Note: these variables have "cmd" or "command" in their name for backward compatibility:
    //       they used to be part of wxCommandEvent, not this mixin.
    wxString          m_cmdString;     // String event argument
    int               m_commandInt;
    long              m_extraLong;     // Additional information (e.g. select/deselect)

    wxDECLARE_NO_ASSIGN_CLASS(wxEventBasicPayloadMixin);
};

class WXDLLIMPEXP_BASE wxEventAnyPayloadMixin : public wxEventBasicPayloadMixin
{
public:
    wxEventAnyPayloadMixin() : wxEventBasicPayloadMixin() {}

    wxDECLARE_NO_ASSIGN_CLASS(wxEventBasicPayloadMixin);
};


// Idle event
/*
 wxEVT_IDLE
 */

// Whether to always send idle events to windows, or
// to only send update events to those with the
// wxWS_EX_PROCESS_IDLE style.

enum wxIdleMode
{
        // Send idle events to all windows
    wxIDLE_PROCESS_ALL,

        // Send idle events to windows that have
        // the wxWS_EX_PROCESS_IDLE flag specified
    wxIDLE_PROCESS_SPECIFIED
};

class WXDLLIMPEXP_BASE wxIdleEvent : public wxEvent
{
public:
    wxIdleEvent()
        : wxEvent(0, wxEVT_IDLE),
          m_requestMore(false)
        { }
    wxIdleEvent(const wxIdleEvent& event)
        : wxEvent(event),
          m_requestMore(event.m_requestMore)
    { }

    void RequestMore(bool needMore = true) { m_requestMore = needMore; }
    bool MoreRequested() const { return m_requestMore; }

    virtual wxEvent *Clone() const { return new wxIdleEvent(*this); }

protected:
    bool m_requestMore;

private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxIdleEvent)
};


// Thread event

class WXDLLIMPEXP_BASE wxThreadEvent : public wxEvent,
                                       public wxEventAnyPayloadMixin
{
public:
    wxThreadEvent(wxEventType eventType = wxEVT_THREAD, int id = wxID_ANY)
        : wxEvent(id, eventType)
        { }

    wxThreadEvent(const wxThreadEvent& event)
        : wxEvent(event),
          wxEventAnyPayloadMixin(event)
    {
        // make sure our string member (which uses COW, aka refcounting) is not
        // shared by other wxString instances:
        SetString(GetString().Clone());
    }

    virtual wxEvent *Clone() const
    {
        return new wxThreadEvent(*this);
    }

private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxThreadEvent)
};


// ============================================================================
// event handler and related classes
// ============================================================================


// struct containing the members common to static and dynamic event tables
// entries
struct WXDLLIMPEXP_BASE wxEventTableEntryBase
{
    wxEventTableEntryBase(int winid, int idLast,
                          wxEventFunctor* fn, wxObject *data)
        : m_id(winid),
          m_lastId(idLast),
          m_fn(fn),
          m_callbackUserData(data) { }

    wxEventTableEntryBase( const wxEventTableEntryBase &entry )
        : m_id( entry.m_id ),
          m_lastId( entry.m_lastId ),
          m_fn( entry.m_fn ),
          m_callbackUserData( entry.m_callbackUserData )
    {
        // This is a 'hack' to ensure that only one instance tries to delete
        // the functor pointer. It is safe as long as the only place where the
        // copy constructor is being called is when the static event tables are
        // being initialized (a temporary instance is created and then this
        // constructor is called).

        const_cast<wxEventTableEntryBase&>( entry ).m_fn = NULL;
    }

    ~wxEventTableEntryBase()
    {
        delete m_fn;
    }

    // the range of ids for this entry: if m_lastId == wxID_ANY, the range
    // consists only of m_id, otherwise it is m_id..m_lastId inclusive
    int m_id,
        m_lastId;

    // function/method/functor to call
    wxEventFunctor* m_fn;

    // arbitrary user data associated with the callback
    wxObject* m_callbackUserData;

private:
    wxDECLARE_NO_ASSIGN_CLASS(wxEventTableEntryBase);
};

// an entry from a static event table
struct WXDLLIMPEXP_BASE wxEventTableEntry : public wxEventTableEntryBase
{
    wxEventTableEntry(const int& evType, int winid, int idLast,
                      wxEventFunctor* fn, wxObject *data)
        : wxEventTableEntryBase(winid, idLast, fn, data),
        m_eventType(evType)
    { }

    // the reference to event type: this allows us to not care about the
    // (undefined) order in which the event table entries and the event types
    // are initialized: initially the value of this reference might be
    // invalid, but by the time it is used for the first time, all global
    // objects will have been initialized (including the event type constants)
    // and so it will have the correct value when it is needed
    const int& m_eventType;

private:
    wxDECLARE_NO_ASSIGN_CLASS(wxEventTableEntry);
};

// an entry used in dynamic event table managed by wxEvtHandler::Connect()
struct WXDLLIMPEXP_BASE wxDynamicEventTableEntry : public wxEventTableEntryBase
{
    wxDynamicEventTableEntry(int evType, int winid, int idLast,
                             wxEventFunctor* fn, wxObject *data)
        : wxEventTableEntryBase(winid, idLast, fn, data),
          m_eventType(evType)
    { }

    // not a reference here as we can't keep a reference to a temporary int
    // created to wrap the constant value typically passed to Connect() - nor
    // do we need it
    int m_eventType;

private:
    wxDECLARE_NO_ASSIGN_CLASS(wxDynamicEventTableEntry);
};

// ----------------------------------------------------------------------------
// wxEventTable: an array of event entries terminated with {0, 0, 0, 0, 0}
// ----------------------------------------------------------------------------

struct WXDLLIMPEXP_BASE wxEventTable
{
    const wxEventTable *baseTable;    // base event table (next in chain)
    const wxEventTableEntry *entries; // bottom of entry array
};

// ----------------------------------------------------------------------------
// wxEventHashTable: a helper of wxEvtHandler to speed up wxEventTable lookups.
// ----------------------------------------------------------------------------

WX_DEFINE_ARRAY_PTR(const wxEventTableEntry*, wxEventTableEntryPointerArray);

// ----------------------------------------------------------------------------
// wxEvtHandler: the base class for all objects handling wxWidgets events
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxEvtHandler : public wxObject
                                    , public wxTrackable
{
public:
    wxEvtHandler();
    virtual ~wxEvtHandler();


    // Event handler chain
    // -------------------

    wxEvtHandler *GetNextHandler() const { return m_nextHandler; }
    wxEvtHandler *GetPreviousHandler() const { return m_previousHandler; }
    virtual void SetNextHandler(wxEvtHandler *handler) { m_nextHandler = handler; }
    virtual void SetPreviousHandler(wxEvtHandler *handler) { m_previousHandler = handler; }

    void Unlink();


    // Global event filters
    // --------------------

    // Add an event filter whose FilterEvent() method will be called for each
    // and every event processed by wxWidgets. The filters are called in LIFO
    // order and wxApp is registered as an event filter by default. The pointer
    // must remain valid until it's removed with RemoveFilter() and is not
    // deleted by wxEvtHandler.
    static void AddFilter(wxEventFilter* filter);

    // Remove a filter previously installed with AddFilter().
    static void RemoveFilter(wxEventFilter* filter);


    // Event queuing and processing
    // ----------------------------

    // Process an event right now: this can only be called from the main
    // thread, use QueueEvent() for scheduling the events for
    // processing from other threads.
    virtual bool ProcessEvent(wxEvent& event);

    // Schedule the given event to be processed later. It takes ownership of
    // the event pointer, i.e. it will be deleted later. This is safe to call
    // from multiple threads although you still need to ensure that wxString
    // fields of the event object are deep copies and not use the same string
    // buffer as other wxString objects in this thread.
    virtual void QueueEvent(wxEvent *event);

    // Add an event to be processed later: notice that this function is not
    // safe to call from threads other than main, use QueueEvent()
    virtual void AddPendingEvent(const wxEvent& event)
    {
        // notice that the thread-safety problem comes from the fact that
        // Clone() doesn't make deep copies of wxString fields of wxEvent
        // object and so the same wxString could be used from both threads when
        // the event object is destroyed in this one -- QueueEvent() avoids
        // this problem as the event pointer is not used any more in this
        // thread at all after it is called.
        QueueEvent(event.Clone());
    }

    void ProcessPendingEvents();
        // NOTE: uses ProcessEvent()

    void DeletePendingEvents();

private:
    static const wxEventTableEntry sm_eventTableEntries[];

protected:
    // this one is called after failing to find the event handle in our own
    // table to give a chance to the other windows to process it
    //
    // base class implementation passes the event to wxTheApp
    virtual bool TryAfter(wxEvent& event);

    static const wxEventTable sm_eventTable;
    virtual const wxEventTable *GetEventTable() const;

    wxEvtHandler*       m_nextHandler;
    wxEvtHandler*       m_previousHandler;
    wxList*             m_pendingEvents;

#if wxUSE_THREADS
    // critical section protecting m_pendingEvents
    wxCriticalSection m_pendingEventsLock;
#endif // wxUSE_THREADS

    // Is event handler enabled?
    bool                m_enabled;

private:
    // pass the event to wxTheApp instance, called from TryAfter()
    bool DoTryApp(wxEvent& event);

    // try to process events in all handlers chained to this one
    bool DoTryChain(wxEvent& event);

    // Head of the event filter linked list.
    static wxEventFilter* ms_filterList;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxEvtHandler)
};

WX_DEFINE_ARRAY_WITH_DECL_PTR(wxEvtHandler *, wxEvtHandlerArray, class WXDLLIMPEXP_BASE);


// Define an inline method of wxObjectEventFunctor which couldn't be defined
// before wxEvtHandler declaration: at least Sun CC refuses to compile function
// calls through pointer to member for forward-declared classes (see #12452).
inline void wxObjectEventFunctor::operator()(wxEvtHandler *handler, wxEvent& event)
{
    wxEvtHandler * const realHandler = m_handler ? m_handler : handler;

    (realHandler->*m_method)(event);
}

typedef void (wxEvtHandler::*wxEventFunction)(wxEvent&);
typedef void (wxEvtHandler::*wxIdleEventFunction)(wxIdleEvent&);
typedef void (wxEvtHandler::*wxThreadEventFunction)(wxThreadEvent&);

#define wxEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxEventFunction, func)
#define wxIdleEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxIdleEventFunction, func)
#define wxThreadEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxThreadEventFunction, func)

// N.B. In GNU-WIN32, you *have* to take the address of a member function
// (use &) or the compiler crashes...

#define wxDECLARE_EVENT_TABLE()                                         \
    private:                                                            \
        static const wxEventTableEntry sm_eventTableEntries[];          \
    protected:                                                          \
        static const wxEventTable        sm_eventTable;                 \
        virtual const wxEventTable*      GetEventTable() const;         \

// N.B.: when building DLL with Borland C++ 5.5 compiler, you must initialize
//       sm_eventTable before using it in GetEventTable() or the compiler gives
//       E2233 (see http://groups.google.com/groups?selm=397dcc8a%241_2%40dnews)

#define wxBEGIN_EVENT_TABLE(theClass, baseClass) \
    const wxEventTable theClass::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass::sm_eventTableEntries[0] }; \
    const wxEventTable *theClass::GetEventTable() const \
        { return &theClass::sm_eventTable; } \
    const wxEventTableEntry theClass::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE1(theClass, baseClass, T1) \
    template<typename T1> \
    const wxEventTable theClass<T1>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1>::sm_eventTableEntries[0] }; \
    template<typename T1> \
    const wxEventTable *theClass<T1>::GetEventTable() const \
        { return &theClass<T1>::sm_eventTable; } \
    template<typename T1> \
    const wxEventTableEntry theClass<T1>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE2(theClass, baseClass, T1, T2) \
    template<typename T1, typename T2> \
    const wxEventTable theClass<T1, T2>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2> \
    const wxEventTable *theClass<T1, T2>::GetEventTable() const \
        { return &theClass<T1, T2>::sm_eventTable; } \
    template<typename T1, typename T2> \
    const wxEventTableEntry theClass<T1, T2>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE3(theClass, baseClass, T1, T2, T3) \
    template<typename T1, typename T2, typename T3> \
    const wxEventTable theClass<T1, T2, T3>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3> \
    const wxEventTable *theClass<T1, T2, T3>::GetEventTable() const \
        { return &theClass<T1, T2, T3>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3> \
    const wxEventTableEntry theClass<T1, T2, T3>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE4(theClass, baseClass, T1, T2, T3, T4) \
    template<typename T1, typename T2, typename T3, typename T4> \
    const wxEventTable theClass<T1, T2, T3, T4>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3, T4>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3, typename T4> \
    const wxEventTable *theClass<T1, T2, T3, T4>::GetEventTable() const \
        { return &theClass<T1, T2, T3, T4>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3, typename T4> \
    const wxEventTableEntry theClass<T1, T2, T3, T4>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE5(theClass, baseClass, T1, T2, T3, T4, T5) \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    const wxEventTable theClass<T1, T2, T3, T4, T5>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3, T4, T5>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    const wxEventTable *theClass<T1, T2, T3, T4, T5>::GetEventTable() const \
        { return &theClass<T1, T2, T3, T4, T5>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    const wxEventTableEntry theClass<T1, T2, T3, T4, T5>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE7(theClass, baseClass, T1, T2, T3, T4, T5, T6, T7) \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
    const wxEventTable theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
    const wxEventTable *theClass<T1, T2, T3, T4, T5, T6, T7>::GetEventTable() const \
        { return &theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
    const wxEventTableEntry theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE8(theClass, baseClass, T1, T2, T3, T4, T5, T6, T7, T8) \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
    const wxEventTable theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
    const wxEventTable *theClass<T1, T2, T3, T4, T5, T6, T7, T8>::GetEventTable() const \
        { return &theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
    const wxEventTableEntry theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTableEntries[] = { \

#define wxEND_EVENT_TABLE() \
    wxDECLARE_EVENT_TABLE_TERMINATOR() };

/*
 * Event table macros
 */

// helpers for writing shorter code below: declare an event macro taking 2, 1
// or none ids (the missing ids default to wxID_ANY)
//
// macro arguments:
//  - evt one of wxEVT_XXX constants
//  - id1, id2 ids of the first/last id
//  - fn the function (should be cast to the right type)
#define wx__DECLARE_EVT2(evt, id1, id2, fn) \
    wxDECLARE_EVENT_TABLE_ENTRY(evt, id1, id2, fn, NULL),
#define wx__DECLARE_EVT1(evt, id, fn) \
    wx__DECLARE_EVT2(evt, id, wxID_ANY, fn)
#define wx__DECLARE_EVT0(evt, fn) \
    wx__DECLARE_EVT1(evt, wxID_ANY, fn)

// Generic events
#define EVT_CUSTOM(event, winid, func) \
    wx__DECLARE_EVT1(event, winid, wxEventHandler(func))
#define EVT_CUSTOM_RANGE(event, id1, id2, func) \
    wx__DECLARE_EVT2(event, id1, id2, wxEventHandler(func))

// EVT_COMMAND
#define EVT_COMMAND(winid, event, func) \
    wx__DECLARE_EVT1(event, winid, wxCommandEventHandler(func))

#define EVT_COMMAND_RANGE(id1, id2, event, func) \
    wx__DECLARE_EVT2(event, id1, id2, wxCommandEventHandler(func))

#define EVT_NOTIFY(event, winid, func) \
    wx__DECLARE_EVT1(event, winid, wxNotifyEventHandler(func))

#define EVT_NOTIFY_RANGE(event, id1, id2, func) \
    wx__DECLARE_EVT2(event, id1, id2, wxNotifyEventHandler(func))

// Idle event
#define EVT_IDLE(func) wx__DECLARE_EVT0(wxEVT_IDLE, wxIdleEventHandler(func))

// Update UI event
#define EVT_UPDATE_UI(winid, func) wx__DECLARE_EVT1(wxEVT_UPDATE_UI, winid, wxUpdateUIEventHandler(func))
#define EVT_UPDATE_UI_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_UPDATE_UI, id1, id2, wxUpdateUIEventHandler(func))

// Help events
#define EVT_HELP(winid, func) wx__DECLARE_EVT1(wxEVT_HELP, winid, wxHelpEventHandler(func))
#define EVT_HELP_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_HELP, id1, id2, wxHelpEventHandler(func))
#define EVT_DETAILED_HELP(winid, func) wx__DECLARE_EVT1(wxEVT_DETAILED_HELP, winid, wxHelpEventHandler(func))
#define EVT_DETAILED_HELP_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_DETAILED_HELP, id1, id2, wxHelpEventHandler(func))

// Context Menu Events
#define EVT_CONTEXT_MENU(func) wx__DECLARE_EVT0(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(func))
#define EVT_COMMAND_CONTEXT_MENU(winid, func) wx__DECLARE_EVT1(wxEVT_CONTEXT_MENU, winid, wxContextMenuEventHandler(func))

// Clipboard text Events
#define EVT_TEXT_CUT(winid, func) wx__DECLARE_EVT1(wxEVT_TEXT_CUT, winid, wxClipboardTextEventHandler(func))
#define EVT_TEXT_COPY(winid, func) wx__DECLARE_EVT1(wxEVT_TEXT_COPY, winid, wxClipboardTextEventHandler(func))
#define EVT_TEXT_PASTE(winid, func) wx__DECLARE_EVT1(wxEVT_TEXT_PASTE, winid, wxClipboardTextEventHandler(func))

// Thread events
#define EVT_THREAD(id, func)  wx__DECLARE_EVT1(wxEVT_THREAD, id, wxThreadEventHandler(func))

// ----------------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------------

// This is an ugly hack to allow the use of Bind() instead of Connect() inside
// the library code if the library was built with support for it, here is how
// it is used:
//
// class SomeEventHandlingClass : wxBIND_OR_CONNECT_HACK_BASE_CLASS
//                                public SomeBaseClass
// {
// public:
//     SomeEventHandlingClass(wxWindow *win)
//     {
//         // connect to the event for the given window
//         wxBIND_OR_CONNECT_HACK(win, wxEVT_SOMETHING, wxSomeEventHandler,
//                                SomeEventHandlingClass::OnSomeEvent, this);
//     }
//
// private:
//     void OnSomeEvent(wxSomeEvent&) { ... }
// };
//
// ----------------------------------------------------------------------------
// Compatibility macro aliases
// ----------------------------------------------------------------------------

// deprecated variants _not_ requiring a semicolon after them and without wx prefix
// (note that also some wx-prefixed macro do _not_ require a semicolon because
//  it's not always possible to force the compire to require it)

#define DECLARE_EVENT_TABLE_ENTRY(type, winid, idLast, fn, obj) \
    wxDECLARE_EVENT_TABLE_ENTRY(type, winid, idLast, fn, obj)
#define DECLARE_EVENT_TABLE_TERMINATOR()               wxDECLARE_EVENT_TABLE_TERMINATOR()
#define DECLARE_EVENT_TABLE()                          wxDECLARE_EVENT_TABLE();
#define BEGIN_EVENT_TABLE(a,b)                         wxBEGIN_EVENT_TABLE(a,b)
#define BEGIN_EVENT_TABLE_TEMPLATE1(a,b,c)             wxBEGIN_EVENT_TABLE_TEMPLATE1(a,b,c)
#define BEGIN_EVENT_TABLE_TEMPLATE2(a,b,c,d)           wxBEGIN_EVENT_TABLE_TEMPLATE2(a,b,c,d)
#define BEGIN_EVENT_TABLE_TEMPLATE3(a,b,c,d,e)         wxBEGIN_EVENT_TABLE_TEMPLATE3(a,b,c,d,e)
#define BEGIN_EVENT_TABLE_TEMPLATE4(a,b,c,d,e,f)       wxBEGIN_EVENT_TABLE_TEMPLATE4(a,b,c,d,e,f)
#define BEGIN_EVENT_TABLE_TEMPLATE5(a,b,c,d,e,f,g)     wxBEGIN_EVENT_TABLE_TEMPLATE5(a,b,c,d,e,f,g)
#define BEGIN_EVENT_TABLE_TEMPLATE6(a,b,c,d,e,f,g,h)   wxBEGIN_EVENT_TABLE_TEMPLATE6(a,b,c,d,e,f,g,h)
#define END_EVENT_TABLE()                              wxEND_EVENT_TABLE()

// other obsolete event declaration/definition macros; we don't need them any longer
// but we keep them for compatibility as it doesn't cost us anything anyhow
#define BEGIN_DECLARE_EVENT_TYPES()
#define END_DECLARE_EVENT_TYPES()
#define DECLARE_EXPORTED_EVENT_TYPE(expdecl, name, value) \
    extern expdecl const wxEventType name;
#define DECLARE_EVENT_TYPE(name, value) \
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_CORE, name, value)
#define DECLARE_LOCAL_EVENT_TYPE(name, value) \
    DECLARE_EXPORTED_EVENT_TYPE(wxEMPTY_PARAMETER_VALUE, name, value)
#define DEFINE_EVENT_TYPE(name) const wxEventType name = wxNewEventType();
#define DEFINE_LOCAL_EVENT_TYPE(name) DEFINE_EVENT_TYPE(name)

// alias for backward compatibility with 2.9.0:
#define wxEVT_COMMAND_THREAD                  wxEVT_THREAD
// other old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_BUTTON_CLICKED          wxEVT_BUTTON
#define wxEVT_COMMAND_CHECKBOX_CLICKED        wxEVT_CHECKBOX
#define wxEVT_COMMAND_CHOICE_SELECTED         wxEVT_CHOICE
#define wxEVT_COMMAND_LISTBOX_SELECTED        wxEVT_LISTBOX
#define wxEVT_COMMAND_LISTBOX_DOUBLECLICKED   wxEVT_LISTBOX_DCLICK
#define wxEVT_COMMAND_CHECKLISTBOX_TOGGLED    wxEVT_CHECKLISTBOX
#define wxEVT_COMMAND_MENU_SELECTED           wxEVT_MENU
#define wxEVT_COMMAND_TOOL_CLICKED            wxEVT_TOOL
#define wxEVT_COMMAND_SLIDER_UPDATED          wxEVT_SLIDER
#define wxEVT_COMMAND_RADIOBOX_SELECTED       wxEVT_RADIOBOX
#define wxEVT_COMMAND_RADIOBUTTON_SELECTED    wxEVT_RADIOBUTTON
#define wxEVT_COMMAND_SCROLLBAR_UPDATED       wxEVT_SCROLLBAR
#define wxEVT_COMMAND_VLBOX_SELECTED          wxEVT_VLBOX
#define wxEVT_COMMAND_COMBOBOX_SELECTED       wxEVT_COMBOBOX
#define wxEVT_COMMAND_TOOL_RCLICKED           wxEVT_TOOL_RCLICKED
#define wxEVT_COMMAND_TOOL_DROPDOWN_CLICKED   wxEVT_TOOL_DROPDOWN
#define wxEVT_COMMAND_TOOL_ENTER              wxEVT_TOOL_ENTER
#define wxEVT_COMMAND_COMBOBOX_DROPDOWN       wxEVT_COMBOBOX_DROPDOWN
#define wxEVT_COMMAND_COMBOBOX_CLOSEUP        wxEVT_COMBOBOX_CLOSEUP
#define wxEVT_COMMAND_TEXT_COPY               wxEVT_TEXT_COPY
#define wxEVT_COMMAND_TEXT_CUT                wxEVT_TEXT_CUT
#define wxEVT_COMMAND_TEXT_PASTE              wxEVT_TEXT_PASTE
#define wxEVT_COMMAND_TEXT_UPDATED            wxEVT_TEXT

#endif // _WX_EVENT_H_
