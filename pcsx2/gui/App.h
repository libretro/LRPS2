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

#include "wxAppWithHelpers.h"

#include <wx/apptrait.h>
#include <memory>

#include "Pcsx2Defs.h"

#include "AppConfig.h"
#include "AppCoreThread.h"
#include "AppGameDatabase.h"
#include "AppEventListeners.h"

#include "../System.h"
#include "SysThreads.h"

class Pcsx2App;

typedef void (Pcsx2App::*FnPtr_Pcsx2App)();

// -------------------------------------------------------------------------------------------
//  pxAppResources
// -------------------------------------------------------------------------------------------
// Container class for resources that should (or must) be unloaded prior to the ~wxApp() destructor.
// (typically this object is deleted at OnExit() or just prior to OnExit()).
//
class pxAppResources
{
public:
	std::unique_ptr<AppGameDatabase>	GameDB;

	pxAppResources();
	virtual ~pxAppResources();
};

class CommandlineOverrides
{
public:
	wxDirName		SettingsFolder;

	bool			DisableSpeedhacks;

	// Note that gamefixes in this array should only be honored if the
	// "HasCustomGamefixes" boolean is also enabled.
	Pcsx2Config::GamefixOptions	Gamefixes;
	bool			ApplyCustomGamefixes;

public:
	CommandlineOverrides()
	{
		DisableSpeedhacks		= false;
		ApplyCustomGamefixes	        = false;
	}
	
	// Returns true if either speedhacks or gamefixes are being overridden.
	bool HasCustomHacks() const
	{
		return DisableSpeedhacks || ApplyCustomGamefixes;
	}
	
	void RemoveCustomHacks()
	{
		DisableSpeedhacks = false;
		ApplyCustomGamefixes = false;
	}
};

// =====================================================================================================
//  Pcsx2App  -  main wxApp class
// =====================================================================================================
class Pcsx2App : public wxAppWithHelpers
{
	typedef wxAppWithHelpers _parent;

	// ----------------------------------------------------------------------------
	// Event Sources!
	// These need to be at the top of the App class, because a lot of other things depend
	// on them and they are, themselves, fairly self-contained.

protected:
	EventSource<IEventListener_CoreThread>	m_evtsrc_CoreThreadStatus;
	EventSource<IEventListener_AppStatus>	m_evtsrc_AppStatus;

public:
	void AddListener( IEventListener_CoreThread& listener )
	{
		m_evtsrc_CoreThreadStatus.Add( listener );
	}

	void AddListener( IEventListener_AppStatus& listener )
	{
		m_evtsrc_AppStatus.Add( listener );
	}

	void RemoveListener( IEventListener_CoreThread& listener )
	{
		m_evtsrc_CoreThreadStatus.Remove( listener );
	}

	void RemoveListener( IEventListener_AppStatus& listener )
	{
		m_evtsrc_AppStatus.Remove( listener );
	}

	void AddListener( IEventListener_CoreThread* listener )
	{
		m_evtsrc_CoreThreadStatus.Add( listener );
	}

	void AddListener( IEventListener_AppStatus* listener )
	{
		m_evtsrc_AppStatus.Add( listener );
	}

	void RemoveListener( IEventListener_CoreThread* listener )
	{
		m_evtsrc_CoreThreadStatus.Remove( listener );
	}

	void RemoveListener( IEventListener_AppStatus* listener )
	{
		m_evtsrc_AppStatus.Remove( listener );
	}
	
	void DispatchEvent( AppEventType evt );
	void DispatchEvent( CoreThreadStatus evt );
	void DispatchUiSettingsEvent();
	void DispatchVmSettingsEvent();

	// ----------------------------------------------------------------------------
protected:
	Threading::Mutex				m_mtx_Resources;
	Threading::Mutex				m_mtx_LoadingGameDB;

public:
	CommandlineOverrides			Overrides;

protected:
	std::unique_ptr<pxAppResources> m_Resources;

public:
	std::unique_ptr<SysCpuProviderPack> m_CpuProviders;
	std::unique_ptr<SysMainMemory> m_VmReserve;

public:
	Pcsx2App();
	virtual ~Pcsx2App();

	void SysApplySettings();
	void SysExecute( CDVD_SourceType cdvdsrc, const wxString& elf_override=wxEmptyString );
	
	SysMainMemory& GetVmReserve();
	// --------------------------------------------------------------------------
	//  Startup / Shutdown Helpers
	// --------------------------------------------------------------------------

	bool DetectCpuAndUserMode();
	void CleanupRestartable();
	void CleanupResources();

	// --------------------------------------------------------------------------
	//  App-wide Resources
	// --------------------------------------------------------------------------
	// All of these accessors cache the resources on first use and retain them in
	// memory until the program exits.
	pxAppResources&		GetResourceCache();
	AppGameDatabase* GetGameDatabase();

	// --------------------------------------------------------------------------
	//  Overrides of wxApp virtuals:
	// --------------------------------------------------------------------------
	wxAppTraits* CreateTraits();
	bool OnInit();
	int  OnExit();
	void CleanUp();

	void AllocateCoreStuffs();
	void CleanupOnExit();
protected:
	bool AppRpc_TryInvoke( FnPtr_Pcsx2App method );
	bool AppRpc_TryInvokeAsync( FnPtr_Pcsx2App method );
};

wxDECLARE_APP(Pcsx2App);

// --------------------------------------------------------------------------------------
//  s* macros!  ['s' stands for 'shortcut']
// --------------------------------------------------------------------------------------
// Use these for "silent fail" invocation of PCSX2 Application-related constructs.  If the
// construct (albeit wxApp, MainFrame, CoreThread, etc) is null, the requested method will
// not be invoked, and an optional "else" clause can be affixed for handling the end case.
//
// Usage Examples:
//   sMainFrame.ApplySettings();
//   sMainFrame.ApplySettings(); else Console.WriteLn( "Judge Wapner" );	// 'else' clause for handling NULL scenarios.
//
// Note!  These macros are not "syntax complete", which means they could generate unexpected
// syntax errors in some situations, and more importantly, they cannot be used for invoking
// functions with return values.
//
// Rationale: There are a lot of situations where we want to invoke a void-style method on
// various volatile object pointers (App, Corethread, MainFrame, etc).  Typically if these
// objects are NULL the most intuitive response is to simply ignore the call request and
// continue running silently.  These macros make that possible without any extra boilerplate
// conditionals or temp variable defines in the code.
//
#define sApp \
	if( Pcsx2App* __app_ = (Pcsx2App*)wxApp::GetInstance() ) (*__app_)

// --------------------------------------------------------------------------------------
//  External App-related Globals and Shortcuts
// --------------------------------------------------------------------------------------
class SysMtgsThread;

extern __aligned16 SysMtgsThread mtgsThread;
extern __aligned16 AppCoreThread CoreThread;
