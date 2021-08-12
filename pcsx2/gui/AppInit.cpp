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
#include "MTVU.h" // for thread cancellation on shutdown

#include <memory>
bool Pcsx2App::DetectCpuAndUserMode()
{
	AffinityAssert_AllowFrom_MainUI();
	
#ifdef _M_X86
	x86caps.Identify();
	x86caps.SIMD_EstablishMXCSRmask();
#endif

	AppConfig_OnChangedSettingsFolder();

	return true;
}

void Pcsx2App::AllocateCoreStuffs()
{
	if( AppRpc_TryInvokeAsync( &Pcsx2App::AllocateCoreStuffs ) ) return;

	AppApplySettings();

	GetVmReserve().ReserveAll();

	if( !m_CpuProviders )
	{
		// FIXME : Some or all of SysCpuProviderPack should be run from the SysExecutor thread,
		// so that the thread is safely blocked from being able to start emulation.

		m_CpuProviders = std::make_unique<SysCpuProviderPack>();

		if( m_CpuProviders->HadSomeFailures( g_Conf->EmuOptions.Cpu.Recompiler ) )
		{
			// HadSomeFailures only returns 'true' if an *enabled* cpu type fails to init.  If
			// the user already has all interps configured, for example, then no point in
			// popping up this dialog.
			Pcsx2Config::RecompilerOptions& recOps = g_Conf->EmuOptions.Cpu.Recompiler;
			
			if( BaseException* ex = m_CpuProviders->GetException_EE() )
			{
				recOps.EnableEE		= false;
			}

			if( BaseException* ex = m_CpuProviders->GetException_IOP() )
			{
				recOps.EnableIOP	= false;
			}

			if( BaseException* ex = m_CpuProviders->GetException_MicroVU0() )
			{
				recOps.UseMicroVU0	= false;
				recOps.EnableVU0	= false;
			}

			if( BaseException* ex = m_CpuProviders->GetException_MicroVU1() )
			{
				recOps.UseMicroVU1	= false;
				recOps.EnableVU1	= false;
			}
		}
	}
}

bool Pcsx2App::OnInit()
{
    return true;
}

// This cleanup procedure can only be called when the App message pump is still active.
// OnExit() must use CleanupOnExit instead.
void Pcsx2App::CleanupRestartable()
{
	AffinityAssert_AllowFrom_MainUI();

	CoreThread.Cancel();
}

// This cleanup handler can be called from OnExit (it doesn't need a running message pump),
// but should not be called from the App destructor.  It's needed because wxWidgets doesn't
// always call OnExit(), so I had to make CleanupRestartable, and then encapsulate it here
// to be friendly to the OnExit scenario (no message pump).
void Pcsx2App::CleanupOnExit()
{
	AffinityAssert_AllowFrom_MainUI();

	try
	{
		CleanupRestartable();
		CleanupResources();
	}
	catch( Exception::CancelEvent& )		{ throw; }
	catch( Exception::RuntimeError& ex )
	{
		// Handle runtime errors gracefully during shutdown.  Mostly these are things
		// that we just don't care about by now, and just want to "get 'er done!" so
		// we can exit the app. ;)

		log_cb(RETRO_LOG_ERROR, "Runtime exception handled during CleanupOnExit:\n" );
		log_cb(RETRO_LOG_ERROR, ex.FormatDiagnosticMessage() );
	}

	// Notice: deleting the plugin manager (unloading plugins) here causes Lilypad to crash,
	// likely due to some pending message in the queue that references lilypad procs.
	// We don't need to unload plugins anyway tho -- shutdown is plenty safe enough for
	// closing out all the windows.  So just leave it be and let the plugins get unloaded
	// during the wxApp destructor. -- air
	
	// FIXME: performing a wxYield() here may fix that problem. -- air
}

void Pcsx2App::CleanupResources()
{
	m_mtx_LoadingGameDB.Wait();
	ScopedLock lock(m_mtx_Resources);
	m_Resources = NULL;
}

int Pcsx2App::OnExit()
{
	CleanupOnExit();
	return wxApp::OnExit();
}

Pcsx2App::Pcsx2App() 
{
}

Pcsx2App::~Pcsx2App()
{
	try {
		vu1Thread.Cancel();
	}
	DESTRUCTOR_CATCHALL
}

void Pcsx2App::CleanUp()
{
}

// ------------------------------------------------------------------------------------------
//  Using the MSVCRT to track memory leaks:
// ------------------------------------------------------------------------------------------
// When exiting PCSX2 normally, the CRT will make a list of all memory that's leaked.  The
// number inside {} can be pasted into the line below to cause MSVC to breakpoint on that
// allocation at the time it's made.  And then using a stacktrace you can figure out what
// leaked! :D
//
// Limitations: Unfortunately, wxWidgets gui uses a lot of heap allocations while handling
// messages, and so any mouse movements will pretty much screw up the leak value.  So to use
// this feature you need to execute pcsx in no-gui mode, and then not move the mouse or use
// the keyboard until you get to the leak. >_<
//
// (but this tool is still better than nothing!)

#ifdef PCSX2_DEBUG
struct CrtDebugBreak
{
	CrtDebugBreak( int spot )
	{
#ifdef __WXMSW__
		_CrtSetBreakAlloc( spot );
#endif
	}
};

//CrtDebugBreak breakAt( 11549 );

#endif
