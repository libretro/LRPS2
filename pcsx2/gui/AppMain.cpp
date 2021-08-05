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

#include "PrecompiledHeader.h"
#include "App.h"
#include "GS.h"

#include "Plugins.h"
#include "ps2/BiosTools.h"

Pcsx2App& wxGetApp() {
   static Pcsx2App pcsx2;
   return pcsx2;
}


std::unique_ptr<AppConfig> g_Conf;

// --------------------------------------------------------------------------------------
//  Pcsx2AppMethodEvent
// --------------------------------------------------------------------------------------
// Unlike pxPingEvent, the Semaphore belonging to this event is typically posted when the
// invoked method is completed.  If the method can be executed in non-blocking fashion then
// it should leave the semaphore postback NULL.
//
class Pcsx2AppMethodEvent : public pxActionEvent
{
	typedef pxActionEvent _parent;
	wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(Pcsx2AppMethodEvent);

protected:
	FnPtr_Pcsx2App	m_Method;

public:
	virtual ~Pcsx2AppMethodEvent() = default;
	virtual Pcsx2AppMethodEvent *Clone() const { return new Pcsx2AppMethodEvent(*this); }

	explicit Pcsx2AppMethodEvent( FnPtr_Pcsx2App method=NULL, SynchronousActionState* sema=NULL )
		: pxActionEvent( sema )
	{
		m_Method = method;
	}

	explicit Pcsx2AppMethodEvent( FnPtr_Pcsx2App method, SynchronousActionState& sema )
		: pxActionEvent( sema )
	{
		m_Method = method;
	}
	
	Pcsx2AppMethodEvent( const Pcsx2AppMethodEvent& src )
		: pxActionEvent( src )
	{
		m_Method = src.m_Method;
	}
		
	void SetMethod( FnPtr_Pcsx2App method )
	{
		m_Method = method;
	}
};


wxIMPLEMENT_DYNAMIC_CLASS( Pcsx2AppMethodEvent, pxActionEvent );

// --------------------------------------------------------------------------------------
//  Pcsx2AppTraits (implementations) 
// --------------------------------------------------------------------------------------
wxAppTraits* Pcsx2App::CreateTraits()
{
	return new Pcsx2AppTraits;
}

// ----------------------------------------------------------------------------
//         Pcsx2App Event Handlers
// ----------------------------------------------------------------------------

extern bool renderswitch;

void DoFmvSwitch(bool on)
{
	if (EmuConfig.Gamefixes.FMVinSoftwareHack)
   {
		CoreThread.Pause();
		renderswitch = !renderswitch;
		CoreThread.Resume();
	}
}

// NOTE: Plugins are *not* applied by this function.  Changes to plugins need to handled
// manually.  The PluginSelectorPanel does this, for example.
void AppApplySettings()
{
	AffinityAssert_AllowFrom_MainUI();
#ifdef __LIBRETRO__
	CoreThread.Pause();
#else
	ScopedCoreThreadPause paused_core;
#endif

	g_Conf->Folders.ApplyDefaults();

	// Ensure existence of necessary documents folders.  Plugins and other parts
	// of PCSX2 rely on them.

	g_Conf->Folders.Cheats.Mkdir();
	g_Conf->Folders.CheatsWS.Mkdir();

	g_Conf->EmuOptions.BiosFilename = g_Conf->FullpathToBios();

	CorePlugins.SetSettingsFolder( GetSettingsFolder().ToString() );

	// Update the compression attribute on the Memcards folder.
	// Memcards generally compress very well via NTFS compression.

	#ifdef __WXMSW__
	NTFS_CompressFile( g_Conf->Folders.MemoryCards.ToString(), g_Conf->McdCompressNTFS );
	#endif
	sApp.DispatchEvent( AppStatus_SettingsApplied );

#ifdef __LIBRETRO__
//	CoreThread.Resume();
#else
	paused_core.AllowResume();
#endif

}

// Invokes the specified Pcsx2App method, or posts the method to the main thread if the calling
// thread is not Main.  Action is blocking.  For non-blocking method execution, use
// AppRpc_TryInvokeAsync.
//
// This function works something like setjmp/longjmp, in that the return value indicates if the
// function actually executed the specified method or not.
//
// Returns:
//   FALSE if the method was not posted to the main thread (meaning this IS the main thread!)
//   TRUE if the method was posted.
//
bool Pcsx2App::AppRpc_TryInvoke( FnPtr_Pcsx2App method )
{
	if( wxThread::IsMain() ) return false;

	SynchronousActionState sync;
	PostEvent( Pcsx2AppMethodEvent( method, sync ) );
	sync.WaitForResult();

	return true;
}

// Invokes the specified Pcsx2App method, or posts the method to the main thread if the calling
// thread is not Main.  Action is non-blocking.  For blocking method execution, use
// AppRpc_TryInvoke.
//
// This function works something like setjmp/longjmp, in that the return value indicates if the
// function actually executed the specified method or not.
//
// Returns:
//   FALSE if the method was not posted to the main thread (meaning this IS the main thread!)
//   TRUE if the method was posted.
//
bool Pcsx2App::AppRpc_TryInvokeAsync( FnPtr_Pcsx2App method )
{
	if( wxThread::IsMain() ) return false;
	PostEvent( Pcsx2AppMethodEvent( method ) );
	return true;
}

// Posts a method to the main thread; non-blocking.  Post occurs even when called from the
// main thread.
void Pcsx2App::PostAppMethod( FnPtr_Pcsx2App method )
{
	PostEvent( Pcsx2AppMethodEvent( method ) );
}

SysMainMemory& Pcsx2App::GetVmReserve()
{
	if (!m_VmReserve) m_VmReserve = std::unique_ptr<SysMainMemory>(new SysMainMemory());
	return *m_VmReserve;
}
void Pcsx2App::SysExecute( CDVD_SourceType cdvdsrc, const wxString& elf_override )
{
	ProcessMethod( AppSaveSettings );

	// if something unloaded plugins since this messages was queued then it's best to ignore
	// it, because apparently too much stuff is going on and the emulation states are wonky.
	if( !CorePlugins.AreLoaded() ) return;

	log_cb(RETRO_LOG_DEBUG, "(SysExecute) received.\n");

	CoreThread.ResetQuick();
	CDVDsys_SetFile(CDVD_SourceType::Iso, g_Conf->CurrentIso );
	CDVDsys_ChangeSource( cdvdsrc);

	if( !CoreThread.HasActiveMachine() )
		CoreThread.SetElfOverride( elf_override );

	CoreThread.Resume();
}

SysMainMemory& GetVmMemory()
{
	return wxGetApp().GetVmReserve();
}

SysCoreThread& GetCoreThread()
{
	return CoreThread;
}

SysMtgsThread& GetMTGS()
{
	return mtgsThread;
}

SysCpuProviderPack& GetCpuProviders()
{
	return *wxGetApp().m_CpuProviders;
}
