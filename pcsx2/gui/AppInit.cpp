/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "App.h"
#include "../MTVU.h" // for thread cancellation on shutdown

#include <memory>

bool Pcsx2App::DetectCpuAndUserMode(void)
{
#ifdef _M_X86
	x86caps.Identify();
	x86caps.SIMD_EstablishMXCSRmask();
#endif

	AppConfig_OnChangedSettingsFolder();

	return true;
}

void Pcsx2App::AllocateCoreStuffs(void)
{
	if( AppRpc_TryInvokeAsync( &Pcsx2App::AllocateCoreStuffs ) ) return;

	AppApplySettings();

	GetVmReserve().ReserveAll();

	if( !m_CpuProviders )
		m_CpuProviders = std::make_unique<SysCpuProviderPack>();
}

bool Pcsx2App::OnInit(void)
{
    return true;
}

// This cleanup procedure can only be called when the App message pump is still active.
// OnExit() must use CleanupOnExit instead.
void Pcsx2App::CleanupRestartable(void)
{
	CoreThread.Cancel();
}

// This cleanup handler can be called from OnExit (it doesn't need a running message pump),
// but should not be called from the App destructor.  It's needed because wxWidgets doesn't
// always call OnExit(), so I had to make CleanupRestartable, and then encapsulate it here
// to be friendly to the OnExit scenario (no message pump).
void Pcsx2App::CleanupOnExit(void)
{
	CleanupRestartable();
	CleanupResources();
}

void Pcsx2App::CleanupResources(void)
{
	m_mtx_LoadingGameDB.Acquire();
	m_mtx_LoadingGameDB.Release();
	ScopedLock lock(m_mtx_Resources);
	m_Resources = NULL;
}

int Pcsx2App::OnExit(void)
{
	CleanupOnExit();
	return wxApp::OnExit();
}

Pcsx2App::Pcsx2App(void)
{
}

Pcsx2App::~Pcsx2App(void)
{
}

void Pcsx2App::CleanUp(void)
{
}
