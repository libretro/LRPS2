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

#include "Plugins.h"
#include "GS.h"
#include "AppConfig.h"

using namespace Threading;

__aligned16 SysCorePlugins CorePlugins;

SysCorePlugins& GetCorePlugins()
{
	return CorePlugins;
}


// --------------------------------------------------------------------------------------
//  CorePluginsEvent
// --------------------------------------------------------------------------------------
class CorePluginsEvent : public pxActionEvent
{
	typedef pxActionEvent _parent;

protected:
	PluginEventType		m_evt;

public:
	virtual ~CorePluginsEvent() = default;
	CorePluginsEvent* Clone() const { return new CorePluginsEvent( *this ); }

	explicit CorePluginsEvent( PluginEventType evt, SynchronousActionState* sema=NULL )
		: pxActionEvent( sema )
	{
		m_evt = evt;
	}

	explicit CorePluginsEvent( PluginEventType evt, SynchronousActionState& sema )
		: pxActionEvent( sema )
	{
		m_evt = evt;
	}

	CorePluginsEvent( const CorePluginsEvent& src )
		: pxActionEvent( src )
	{
		m_evt = src.m_evt;
	}
	
	void SetEventType( PluginEventType evt ) { m_evt = evt; }
	PluginEventType GetEventType() { return m_evt; }

protected:
	void InvokeEvent()
	{
		sApp.DispatchEvent( m_evt );
	}
};

// --------------------------------------------------------------------------------------
//  Public API / Interface
// --------------------------------------------------------------------------------------

static void _LoadPluginsImmediate()
{
	if( CorePlugins.AreLoaded() ) return;
	CorePlugins.Load(  );
}

void LoadPluginsImmediate()
{
	AffinityAssert_AllowFrom_SysExecutor();
	_LoadPluginsImmediate();
}
