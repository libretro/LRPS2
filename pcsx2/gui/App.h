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

#include "Utilities/wxAppWithHelpers.h"

#include <wx/fileconf.h>
#include <wx/apptrait.h>
#include <memory>

#include "AppCommon.h"
#include "AppCoreThread.h"

#include "System.h"
#include "System/SysThreads.h"

typedef void FnType_OnThreadComplete(const wxCommandEvent& evt);
typedef void (Pcsx2App::*FnPtr_Pcsx2App)();

// This is used when the GS plugin is handling its own window.  Messages from the PAD
// are piped through to an app-level message handler, which dispatches them through
// the universal Accelerator table.
static const int pxID_PadHandler_Keydown = 8030;

// ------------------------------------------------------------------------
// All Menu Options for the Main Window! :D
// ------------------------------------------------------------------------

enum TopLevelMenuIndices
{
	TopLevelMenu_Pcsx2 = 0,
	TopLevelMenu_Cdvd,
	TopLevelMenu_Config,
	TopLevelMenu_Window,
	TopLevelMenu_Capture,
	TopLevelMenu_Help
};

enum MenuIdentifiers
{
	// Main Menu Section
	MenuId_Boot = 1,
	MenuId_Emulation,
	MenuId_Config,				// General config, plus non audio/video plugins.
	MenuId_Video,				// Video options filled in by GS plugin
	MenuId_Audio,				// audio options filled in by SPU2 plugin
	MenuId_Misc,				// Misc options and help!

	MenuId_Exit = wxID_EXIT,
	MenuId_About = wxID_ABOUT,

	MenuId_EndTopLevel = 20,

	// Run SubSection
	MenuId_Cdvd_Source,
	MenuId_Src_Iso,
	MenuId_Src_Disc,
	MenuId_Src_NoDisc,
	MenuId_Boot_Iso,			// Opens submenu with Iso browser, and recent isos.
	MenuId_RecentIsos_reservedStart,
	MenuId_IsoBrowse = MenuId_RecentIsos_reservedStart + 100,			// Open dialog, runs selected iso.
	MenuId_IsoClear,
	MenuId_DriveSelector,
	MenuId_DriveListRefresh,
	MenuId_Ask_On_Booting,
	MenuId_Boot_CDVD,
	MenuId_Boot_CDVD2,
	MenuId_Boot_ELF,
	//MenuId_Boot_Recent,			// Menu populated with recent source bootings


	MenuId_Sys_SuspendResume,	// suspends/resumes active emulation, retains plugin states
	MenuId_Sys_Shutdown,		// Closes virtual machine, shuts down plugins, wipes states.
	MenuId_Sys_LoadStates,		// Opens load states submenu
	MenuId_Sys_SaveStates,		// Opens save states submenu
	MenuId_EnableBackupStates,	// Checkbox to enable/disables savestates backup
	MenuId_GameSettingsSubMenu,
	MenuId_EnablePatches,
	MenuId_EnableCheats,
	MenuId_EnableIPC,
	MenuId_EnableWideScreenPatches,
	MenuId_EnableInputRecording,
	MenuId_EnableLuaTools,
	MenuId_EnableHostFs,

	MenuId_State_Load,
	MenuId_State_LoadFromFile,
	MenuId_State_Load01,		// first of many load slots
	MenuId_State_LoadBackup = MenuId_State_Load01+20,
	MenuId_State_Save,
	MenuId_State_SaveToFile,
	MenuId_State_Save01,		// first of many save slots

	MenuId_State_EndSlotSection = MenuId_State_Save01+20,

	// Config Subsection
	MenuId_Config_SysSettings,
	MenuId_Config_McdSettings,
	MenuId_Config_AppSettings,
	MenuId_Config_BIOS,
	MenuId_Config_Language,

	// Plugin ID order is important.  Must match the order in tbl_PluginInfo.
	MenuId_Config_GS,
	MenuId_Config_PAD,
	MenuId_Config_SPU2,
	MenuId_Config_CDVD,
	MenuId_Config_USB,
	MenuId_Config_FireWire,
	MenuId_Config_DEV9,
	MenuId_Config_Patches,

	MenuId_Config_Multitap0Toggle,
	MenuId_Config_Multitap1Toggle,
	MenuId_Config_FastBoot,

	MenuId_Help_GetStarted,
	MenuId_Help_Compatibility,
	MenuId_Help_Forums,
	MenuId_Help_Website,
	MenuId_Help_Wiki,
	MenuId_Help_Github,

	// Plugin Sections
	// ---------------
	// Each plugin menu begins with its name, which is a grayed out option that's
	// intended for display purposes only.  Plugin ID sections are spaced out evenly
	// at intervals to make it easy to use a single for-loop to create them.

	MenuId_PluginBase_Name = 0x100,
	MenuId_PluginBase_Settings = 0x101,

	MenuId_Video_CoreSettings = 0x200,// includes frame timings and skippings settings
	MenuId_Video_WindowSettings,

	// Miscellaneous Menu!  (Misc)
	MenuId_Console,				// Enable console
	MenuId_ChangeLang,			// Change language (resets first time wizard to show on next start)
	MenuId_Console_Stdio,		// Enable Stdio

	// Debug Subsection
	MenuId_Debug_Open,			// opens the debugger window / starts a debug session
	MenuId_Debug_MemoryDump,
	MenuId_Debug_Logging,		// dialog for selection additional log options
	MenuId_Debug_CreateBlockdump,
	MenuId_Config_ResetAll,

	// Capture Subsection
	MenuId_Capture_Video,
	MenuId_Capture_Video_Record,
	MenuId_Capture_Video_Stop,
	MenuId_Capture_Screenshot,
};

namespace Exception
{
	// --------------------------------------------------------------------------
	// Exception used to perform an "errorless" termination of the app during OnInit
	// procedures.  This happens when a user cancels out of startup prompts/wizards.
	//
	class StartupAborted : public CancelEvent
	{
		DEFINE_RUNTIME_EXCEPTION( StartupAborted, CancelEvent, L"Startup initialization was aborted by the user." )

	public:
		StartupAborted( const wxString& reason )
		{
			m_message_diag = L"Startup aborted: " + reason;
		}
	};

}

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

class StartupOptions
{
public:
	bool			ForceWizard;
	bool			ForceConsole;
	bool			PortableMode;

	// Disables the fast boot option when auto-running games.  This option only applies
	// if SysAutoRun is also true.
	bool			NoFastBoot;

	// Specifies the Iso file to boot; used only if SysAutoRun is enabled and CdvdSource
	// is set to ISO.
	wxString		IsoFile;

	wxString		ElfFile;

	wxString		GameLaunchArgs;

	// Specifies the CDVD source type to use when AutoRunning
	CDVD_SourceType CdvdSource;

	// Indicates if PCSX2 should autorun the configured CDVD source and/or ISO file.
	bool			SysAutoRun;
	bool			SysAutoRunElf;
	bool			SysAutoRunIrx;

	StartupOptions()
	{
		ForceWizard				= false;
		ForceConsole			= false;
		PortableMode			= false;
		NoFastBoot				= false;
		SysAutoRun				= false;
		SysAutoRunElf			= false;
		SysAutoRunIrx			= false;
		CdvdSource				= CDVD_SourceType::NoDisc;
	}
};

class CommandlineOverrides
{
public:
	AppConfig::FilenameOptions	Filenames;
	wxDirName		SettingsFolder;
	wxFileName		VmSettingsFile;

	bool			DisableSpeedhacks;
	bool			ProfilingMode;

	// Note that gamefixes in this array should only be honored if the
	// "HasCustomGamefixes" boolean is also enabled.
	Pcsx2Config::GamefixOptions	Gamefixes;
	bool			ApplyCustomGamefixes;

public:
	CommandlineOverrides()
	{
		DisableSpeedhacks		= false;
		ApplyCustomGamefixes	= false;
		ProfilingMode			= false;
	}
	
	// Returns TRUE if either speedhacks or gamefixes are being overridden.
	bool HasCustomHacks() const
	{
		return DisableSpeedhacks || ApplyCustomGamefixes;
	}
	
	void RemoveCustomHacks()
	{
		DisableSpeedhacks = false;
		ApplyCustomGamefixes = false;
	}

	bool HasSettingsOverride() const
	{
		return SettingsFolder.IsOk() || VmSettingsFile.IsOk();
	}

	bool HasPluginsOverride() const
	{
		for( int i=0; i<PluginId_Count; ++i )
			if( Filenames.Plugins[i].IsOk() ) return true;

		return false;
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
	EventSource<IEventListener_Plugins>		m_evtsrc_CorePluginStatus;
	EventSource<IEventListener_CoreThread>	m_evtsrc_CoreThreadStatus;
	EventSource<IEventListener_AppStatus>	m_evtsrc_AppStatus;

public:
	void AddListener( IEventListener_Plugins& listener )
	{
		m_evtsrc_CorePluginStatus.Add( listener );	
	}

	void AddListener( IEventListener_CoreThread& listener )
	{
		m_evtsrc_CoreThreadStatus.Add( listener );
	}

	void AddListener( IEventListener_AppStatus& listener )
	{
		m_evtsrc_AppStatus.Add( listener );
	}

	void RemoveListener( IEventListener_Plugins& listener )
	{
		m_evtsrc_CorePluginStatus.Remove( listener );	
	}

	void RemoveListener( IEventListener_CoreThread& listener )
	{
		m_evtsrc_CoreThreadStatus.Remove( listener );
	}

	void RemoveListener( IEventListener_AppStatus& listener )
	{
		m_evtsrc_AppStatus.Remove( listener );
	}

	void AddListener( IEventListener_Plugins* listener )
	{
		m_evtsrc_CorePluginStatus.Add( listener );	
	}

	void AddListener( IEventListener_CoreThread* listener )
	{
		m_evtsrc_CoreThreadStatus.Add( listener );
	}

	void AddListener( IEventListener_AppStatus* listener )
	{
		m_evtsrc_AppStatus.Add( listener );
	}

	void RemoveListener( IEventListener_Plugins* listener )
	{
		m_evtsrc_CorePluginStatus.Remove( listener );	
	}

	void RemoveListener( IEventListener_CoreThread* listener )
	{
		m_evtsrc_CoreThreadStatus.Remove( listener );
	}

	void RemoveListener( IEventListener_AppStatus* listener )
	{
		m_evtsrc_AppStatus.Remove( listener );
	}
	
	void DispatchEvent( PluginEventType evt );
	void DispatchEvent( AppEventType evt );
	void DispatchEvent( CoreThreadStatus evt );
	void DispatchUiSettingsEvent( IniInterface& ini );
	void DispatchVmSettingsEvent( IniInterface& ini );

	// ----------------------------------------------------------------------------
protected:
	int								m_PendingSaves;

	Threading::Mutex				m_mtx_Resources;
	Threading::Mutex				m_mtx_LoadingGameDB;

public:
	StartupOptions					Startup;
	CommandlineOverrides			Overrides;

protected:
	std::unique_ptr<pxAppResources> m_Resources;

public:
#ifndef __LIBRETRO__
	// Executor Thread for complex VM/System tasks.  This thread is used to execute such tasks
	// in parallel to the main message pump, to allow the main pump to run without fear of
	// blocked threads stalling the GUI.
	ExecutorThread					SysExecutorThread;
#endif
	std::unique_ptr<SysCpuProviderPack> m_CpuProviders;
	std::unique_ptr<SysMainMemory> m_VmReserve;

public:
	Pcsx2App();
	virtual ~Pcsx2App();

	void PostAppMethod( FnPtr_Pcsx2App method );

	void SysApplySettings();
	void SysExecute( CDVD_SourceType cdvdsrc, const wxString& elf_override=wxEmptyString );
	
	SysMainMemory& GetVmReserve();
	
	void enterDebugMode();
	void leaveDebugMode();
	void resetDebugger();

	// --------------------------------------------------------------------------
	//  Startup / Shutdown Helpers
	// --------------------------------------------------------------------------

	void DetectCpuAndUserMode();
	void OpenProgramLog();
	void CleanupRestartable();
	void CleanupResources();
	bool TestUserPermissionsRights( const wxDirName& testFolder, wxString& createFailedStr, wxString& accessFailedStr );
	void EstablishAppUserMode();

	wxConfigBase* OpenInstallSettingsFile();
	wxConfigBase* TestForPortableInstall();

	bool HasPendingSaves() const;
	void StartPendingSave();
	void ClearPendingSave();
	
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

	void OnInitCmdLine( wxCmdLineParser& parser );
	bool OnCmdLineParsed( wxCmdLineParser& parser );
	bool OnCmdLineError( wxCmdLineParser& parser );
	bool ParseOverrides( wxCmdLineParser& parser );

	void AllocateCoreStuffs();
	void CleanupOnExit();
protected:
	bool AppRpc_TryInvoke( FnPtr_Pcsx2App method );
	bool AppRpc_TryInvokeAsync( FnPtr_Pcsx2App method );

	void InitDefaultGlobalAccelerators();
	bool TryOpenConfigCwd();

protected:
	// ----------------------------------------------------------------------------
	//      Override wx default exception handling behavior
	// ----------------------------------------------------------------------------

	// Just rethrow exceptions in the main loop, so that we can handle them properly in our
	// custom catch clauses in OnRun().  (ranting note: wtf is the point of this functionality
	// in wx?  Why would anyone ever want a generic catch-all exception handler that *isn't*
	// the unhandled exception handler?  Using this as anything besides a re-throw is terrible
	// program design and shouldn't even be allowed -- air)
	bool OnExceptionInMainLoop() { throw; }

	// Just rethrow unhandled exceptions to cause immediate debugger fail.
	void OnUnhandledException() { throw; }
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

extern void LoadPluginsImmediate();
extern void UnloadPlugins();
extern void ShutdownPlugins();

extern bool SysHasValidState();
extern void SysUpdateIsoSrcFile( const wxString& newIsoFile );
extern void SysUpdateDiscSrcDrive( const wxString& newDiscDrive );
extern void SysStatus( const wxString& text );

extern __aligned16 SysMtgsThread mtgsThread;
extern __aligned16 AppCoreThread CoreThread;
extern __aligned16 SysCorePlugins CorePlugins;

extern void UI_DisableStateActions();
extern void UI_EnableStateActions();

extern void UI_DisableSysActions();
extern void UI_EnableSysActions();

extern void UI_DisableSysShutdown();

#define AffinityAssert_AllowFrom_SysExecutor()
#define AffinityAssert_DisallowFrom_SysExecutor()
