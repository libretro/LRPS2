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

#include "Utilities/PersistentThread.h"
#include "Utilities/pxEvents.h"
#include <memory>

// TODO!!  Make the system defined in this header system a bit more generic, and then move
// it to the Utilities library.

// --------------------------------------------------------------------------------------
//  SysExecEvent
// --------------------------------------------------------------------------------------
// Base class for all pxEvtQueue processable events.
//
// Rules for deriving:
//  * Override InvokeEvent(), *NOT* _DoInvokeEvent().  _DoInvokeEvent() performs setup and
//    wraps exceptions for transport to the invoking context/thread, and then itself calls
//    InvokeEvent() to perform the derived class implementation.
//
//  * Derived classes must implement their own versions of an empty constructor and
//    Clone(), or else the class will fail to be copied to the event handler's thread
//    context correctly.
//
//  * This class is not abstract, and gives no error if the invocation method is not
//    overridden:  It can be used as a simple ping device against the event queue,  Re-
//    awaking the invoking thread as soon as the queue has caught up to and processed
//    the event.
//
//  * Avoid using virtual class inheritence to 'cleverly' bind a SysExecEvent to another
//    existing class.  Multiple inheritence is unreliable at best, and virtual inheritence
//    is even worse.  Create a SysExecEvent wrapper class instead, and embed an instance of
//    the other class you want to turn into an event within it.  It might feel like more work
//    but it *will* be less work in the long run.
//
class SysExecEvent : public ICloneable
{
protected:
	SynchronousActionState*		m_sync;

public:
	virtual ~SysExecEvent() = default;
	SysExecEvent* Clone() const { return new SysExecEvent( *this ); }

	SysExecEvent( SynchronousActionState* sync=NULL )
	{
		m_sync = sync;
	}

	SysExecEvent( SynchronousActionState& sync )
	{
		m_sync = &sync;
	}

	const SynchronousActionState* GetSyncState() const { return m_sync; }
	SynchronousActionState* GetSyncState() { return m_sync; }

	SysExecEvent& SetSyncState( SynchronousActionState* obj ) { m_sync = obj;	return *this; }
	SysExecEvent& SetSyncState( SynchronousActionState& obj ) { m_sync = &obj;	return *this; }

	// Tells the Event Handler whether or not this event can be skipped when the system
	// is being quit or reset.  Typically set this to true for events which shut down the
	// system, since program crashes can occur if the program tries to exit while threads
	// are running.
	virtual bool IsCriticalEvent() const { return false; }
	
	// Tells the Event Handler whether or not this event can be canceled.  Typically events
	// should not prohibit cancellation, since it expedites program termination and helps
	// avoid permanent deadlock.  Some actions like saving states and shutdown procedures
	// should not allow cancellation since they could result in program crashes or corrupted
	// data.
	virtual bool AllowCancelOnExit() const { return true; }
};

// --------------------------------------------------------------------------------------
//  SysExecEvent_MethodVoid
// --------------------------------------------------------------------------------------
class SysExecEvent_MethodVoid : public SysExecEvent
{
protected:
	FnType_Void*	m_method;
	bool			m_IsCritical;
	wxString		m_TraceName;

public:
	wxString GetEventName() const { return m_TraceName; }

	virtual ~SysExecEvent_MethodVoid() = default;
	SysExecEvent_MethodVoid* Clone() const { return new SysExecEvent_MethodVoid( *this ); }

	bool AllowCancelOnExit() const { return !m_IsCritical; }
	bool IsCriticalEvent() const { return m_IsCritical; }

	explicit SysExecEvent_MethodVoid( FnType_Void* method = NULL, const wxChar* traceName=NULL )	
		: m_TraceName( traceName ? traceName : L"VoidMethod" )
	{
		m_method = method;
		m_IsCritical = false;
	}
	
	SysExecEvent_MethodVoid& Critical()
	{
		m_IsCritical = true;
		return *this;
	}
};
