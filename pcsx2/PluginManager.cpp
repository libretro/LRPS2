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
#include "IopCommon.h"

#include <wx/wx.h>
#include <memory>

#include "GS.h"
#include "Gif.h"

#include "gui/MemoryCardFile.h"
#include "Utilities/pxStreams.h"

#include "svnrev.h"

// --------------------------------------------------------------------------------------
//  PCSX2 Callbacks passed to Plugins
// --------------------------------------------------------------------------------------
// This is currently unimplemented, and should be provided by the AppHost (gui) rather
// than the EmuCore.  But as a quickhackfix until the new plugin API is fleshed out, this
// will suit our needs nicely. :)

static BOOL PS2E_CALLBACK pcsx2_GetInt( const char* name, int* dest )
{
	return FALSE;		// not implemented...
}

static BOOL PS2E_CALLBACK pcsx2_GetBoolean( const char* name, BOOL* result )
{
	return FALSE;		// not implemented...
}

static BOOL PS2E_CALLBACK pcsx2_GetString( const char* name, char* dest, int maxlen )
{
	return FALSE;		// not implemented...
}

static char* PS2E_CALLBACK pcsx2_GetStringAlloc( const char* name, void* (PS2E_CALLBACK* allocator)(int size) )
{
	return FALSE;		// not implemented...
}

static void PS2E_CALLBACK pcsx2_OSD_WriteLn( int icon, const char* msg )
{
	log_cb(RETRO_LOG_INFO, msg );
}

// ---------------------------------------------------------------------------------
//  PluginStatus_t Implementations
// ---------------------------------------------------------------------------------
SysCorePlugins::SysCorePlugins()
{
}

// =====================================================================================
//  SysCorePlugins Implementations
// =====================================================================================

SysCorePlugins::~SysCorePlugins()
{
	try
	{
		Unload();
	}
	DESTRUCTOR_CATCHALL

	// All library unloading done automatically by wx.
}

void SysCorePlugins::Load( )
{
	if( !NeedsLoad() ) return;

	m_info = std::make_unique<PluginStatus_t>();

	log_cb(RETRO_LOG_INFO, "Bound GS\n");
	log_cb(RETRO_LOG_INFO, "Plugins loaded successfully.\n" );
}

void SysCorePlugins::Unload()
{
	if( NeedsShutdown() )
		log_cb(RETRO_LOG_WARN, "(SysCorePlugins) Warning: Unloading plugins prior to shutdown!\n");
	if( !NeedsUnload() ) return;

	log_cb(RETRO_LOG_DEBUG, "Unloading plugins...\n" );

	m_info = nullptr;

	log_cb(RETRO_LOG_DEBUG, "Plugins unloaded successfully.\n" );
}

// Exceptions:
//   FileNotFound - Thrown if one of the configured plugins doesn't exist.
//   NotPcsxPlugin - Thrown if one of the configured plugins is an invalid or unsupported DLL

void SysCorePlugins::Open()
{
	Init();

	if( !NeedsOpen() ) return;		// Spam stopper:  returns before writing any logs. >_<

	log_cb(RETRO_LOG_INFO, "Opening plugins...\n");

	if( IsOpen() ) return;

	log_cb(RETRO_LOG_INFO, "Opening GS\n");

	// Each Open needs to be called explicitly. >_<
	GetMTGS().Resume();

	if( m_info ) m_info->IsOpened = true;
	GetMTGS().WaitForOpen();

	log_cb(RETRO_LOG_INFO, "Plugins opened successfully.\n");
}

void SysCorePlugins::Close()
{
	if( !NeedsClose() ) return;	// Spam stopper; returns before writing any logs. >_<

	// Close plugins in reverse order of the initialization procedure, which
	// ensures the GS gets closed last.

	log_cb(RETRO_LOG_INFO, "Closing plugins...\n" );

	if( !IsOpen() ) return;
	
	if( GetMTGS().IsSelf() )
	{
		GSclose();
	}
	else
	{
		log_cb(RETRO_LOG_INFO, "Closing GS\n");
		GetMTGS().Suspend();
	}

	if( m_info ) m_info->IsOpened = false;
	
	log_cb(RETRO_LOG_INFO, "Plugins closed successfully.\n");
}

// Initializes all plugins.  Plugin initialization should be done once for every new emulation
// session.  During a session emulation can be paused/resumed using Open/Close, and should be
// terminated using Shutdown().
//
// Returns TRUE if an init was performed for any (or all) plugins.  Returns FALSE if all
// plugins were already in an initialized state (no action taken).
//
// In a purist emulation sense, Init() and Shutdown() should only ever need be called for when
// the PS2's hardware has received a *full* hard reset.  Soft resets typically should rely on
// the PS2's bios/kernel to re-initialize hardware on the fly.
//
bool SysCorePlugins::Init()
{
	if( !NeedsInit() ) return false;

	log_cb(RETRO_LOG_INFO, "Initializing plugins...\n");

	if( !m_info || m_info->IsInitialized ) return false;

	log_cb(RETRO_LOG_INFO, "Init GS\n");
	if( 0 != GSinit() )
	{
		log_cb(RETRO_LOG_ERROR, "Could not initialize GS.\n");
		return false;
	}

	m_info->IsInitialized = true;

	log_cb(RETRO_LOG_INFO, "Plugins initialized successfully.\n" );

	return true;
}


// Shuts down all plugins.  Plugins are closed first, if necessary.
// Returns TRUE if a shutdown was performed for any (or all) plugins.  Returns FALSE if all
// plugins were already in shutdown state (no action taken).
//
// In a purist emulation sense, Init() and Shutdown() should only ever need be called for when
// the PS2's hardware has received a *full* hard reset.  Soft resets typically should rely on
// the PS2's bios/kernel to re-initialize hardware on the fly.
//
bool SysCorePlugins::Shutdown()
{
	if( !NeedsShutdown() ) return false;

	GetMTGS().Cancel();	// cancel it for speedier shutdown!
	
	log_cb(RETRO_LOG_INFO, "Shutting down plugins...\n");

	if( !m_info || !m_info->IsInitialized )
		return false;
#ifndef NDEBUG
	log_cb(RETRO_LOG_DEBUG, "Shutdown GS\n");
#endif
	GSshutdown();
	m_info->IsInitialized = false;

	log_cb(RETRO_LOG_INFO, "Plugins shutdown successfully.\n");
	
	return true;
}

bool SysCorePlugins::AreLoaded() const
{
	if( !m_info )
		return false;
	return true;
}

bool SysCorePlugins::IsOpen() const
{
	return m_info && m_info->IsInitialized && m_info->IsOpened;
}

bool SysCorePlugins::IsInitialized() const
{
	return m_info && m_info->IsInitialized;
}

bool SysCorePlugins::IsLoaded() const
{
	return !!m_info;
}

bool SysCorePlugins::NeedsLoad() const
{
	return !IsLoaded();
}		

bool SysCorePlugins::NeedsUnload() const
{
	return IsLoaded();
}		

bool SysCorePlugins::NeedsInit() const
{
	return !IsInitialized();
}

bool SysCorePlugins::NeedsShutdown() const
{
	return IsInitialized();
}

bool SysCorePlugins::NeedsOpen() const
{
	return !IsOpen();
}

bool SysCorePlugins::NeedsClose() const
{
	return IsOpen();
}
