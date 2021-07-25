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
#include "Utilities/pxEvents.h"

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
	AppStatus_UiSettingsSaved,
	AppStatus_VmSettingsLoaded,
	AppStatus_VmSettingsSaved,

	AppStatus_SettingsApplied,
	AppStatus_Exiting
};

enum PluginEventType
{
	CorePlugins_Loaded,
	CorePlugins_Init,
	CorePlugins_Opening,		// dispatched prior to plugins being opened
	CorePlugins_Opened,			// dispatched after plugins are opened
	CorePlugins_Closing,		// dispatched prior to plugins being closed
	CorePlugins_Closed,			// dispatched after plugins are closed
	CorePlugins_Shutdown,
	CorePlugins_Unloaded,
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
	IniInterface&	m_ini;

	AppSettingsEventInfo( IniInterface&	ini, AppEventType evt_type );

	IniInterface& GetIni() const
	{
		return const_cast<IniInterface&>(m_ini);
	}
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
//  IEventListener_Plugins
// --------------------------------------------------------------------------------------
class IEventListener_Plugins : public IEventDispatcher<PluginEventType>
{
public:
	typedef PluginEventType EvtParams;

public:
	virtual ~IEventListener_Plugins() = default;

	virtual void DispatchEvent( const PluginEventType& pevt );

protected:
	virtual void CorePlugins_OnLoaded() {}
	virtual void CorePlugins_OnInit() {}
	virtual void CorePlugins_OnOpening() {}		// dispatched prior to plugins being opened
	virtual void CorePlugins_OnOpened() {}		// dispatched after plugins are opened
	virtual void CorePlugins_OnClosing() {}		// dispatched prior to plugins being closed
	virtual void CorePlugins_OnClosed() {}		// dispatched after plugins are closed
	virtual void CorePlugins_OnShutdown() {}
	virtual void CorePlugins_OnUnloaded() {}
};

class EventListener_Plugins : public IEventListener_Plugins
{
public:
	EventListener_Plugins();
	virtual ~EventListener_Plugins();
};

// --------------------------------------------------------------------------------------
//  IEventListener_AppStatus
// --------------------------------------------------------------------------------------
class IEventListener_AppStatus : public IEventDispatcher<AppEventInfo>
{
public:
	typedef AppEventInfo EvtParams;

public:
	virtual ~IEventListener_AppStatus() = default;

	virtual void DispatchEvent( const AppEventInfo& evtinfo );

protected:
	virtual void AppStatusEvent_OnUiSettingsLoadSave( const AppSettingsEventInfo& evtinfo ) {}
	virtual void AppStatusEvent_OnVmSettingsLoadSave( const AppSettingsEventInfo& evtinfo ) {}

	virtual void AppStatusEvent_OnSettingsApplied() {}
	virtual void AppStatusEvent_OnExit() {}
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
	CoreThreadStatus		m_evt;

public:
	virtual ~CoreThreadStatusEvent() = default;
	CoreThreadStatusEvent* Clone() const { return new CoreThreadStatusEvent( *this ); }

	explicit CoreThreadStatusEvent( CoreThreadStatus evt, SynchronousActionState* sema=NULL );
	explicit CoreThreadStatusEvent( CoreThreadStatus evt, SynchronousActionState& sema );

	CoreThreadStatus GetEventType() { return m_evt; }
};
