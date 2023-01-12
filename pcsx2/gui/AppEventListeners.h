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

#include "Utilities/EventSource.h"

enum CoreThreadStatus
{
	CoreThread_Indeterminate,
	CoreThread_Started,
	CoreThread_Resumed,
	CoreThread_Suspended,
	CoreThread_Reset,
	CoreThread_Stopped,
};

enum AppEventType
{
	AppStatus_UiSettingsLoaded,
	AppStatus_VmSettingsLoaded,
	AppStatus_SettingsApplied
};

struct AppEventInfo
{
	AppEventType	evt_type;

	AppEventInfo( AppEventType type )
	{
		evt_type = type;
	}
};

struct AppSettingsEventInfo : AppEventInfo
{
	AppSettingsEventInfo(AppEventType evt_type );
};

// --------------------------------------------------------------------------------------
//  IEventListener_CoreThread
// --------------------------------------------------------------------------------------
class IEventListener_CoreThread : public IEventDispatcher<CoreThreadStatus>
{
public:
	typedef CoreThreadStatus EvtParams;

public:
	virtual ~IEventListener_CoreThread() = default;

	virtual void DispatchEvent( const CoreThreadStatus& status );

protected:
	virtual void CoreThread_OnStarted() {}
	virtual void CoreThread_OnResumed() {}
	virtual void CoreThread_OnSuspended() {}
	virtual void CoreThread_OnReset() {}
	virtual void CoreThread_OnStopped() {}
};

class EventListener_CoreThread : public IEventListener_CoreThread
{
public:
	EventListener_CoreThread();
	virtual ~EventListener_CoreThread();
};

// --------------------------------------------------------------------------------------
//  IEventListener_AppStatus
// --------------------------------------------------------------------------------------
class IEventListener_AppStatus : public IEventDispatcher<AppEventInfo>
{
public:
	typedef AppEventInfo EvtParams;
	virtual ~IEventListener_AppStatus() = default;
};

class EventListener_AppStatus : public IEventListener_AppStatus
{
public:
	EventListener_AppStatus();
	virtual ~EventListener_AppStatus();
};

// --------------------------------------------------------------------------------------
//  CoreThreadStatusEvent
// --------------------------------------------------------------------------------------
class CoreThreadStatusEvent : public pxActionEvent
{
	typedef pxActionEvent _parent;

protected:
	CoreThreadStatus	m_evt;

public:
	virtual ~CoreThreadStatusEvent() = default;
	CoreThreadStatusEvent* Clone() const { return new CoreThreadStatusEvent( *this ); }

	explicit CoreThreadStatusEvent( CoreThreadStatus evt, SynchronousActionState* sema=NULL );
	explicit CoreThreadStatusEvent( CoreThreadStatus evt, SynchronousActionState& sema );
};
