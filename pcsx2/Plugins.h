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

#define PLUGINtypedefs
#define PLUGINfuncs

#include "PS2Edefs.h"
#include "PluginCallbacks.h"

#include "gui/MemoryCardFile.h"
#include "Utilities/Threading.h"

#ifdef _MSC_VER

// Disabling C4673: throwing 'Exception::Blah' the following types will not be considered at the catch site
//   The warning is bugged, and happens even though we're properly inheriting classes with
//   'virtual' qualifiers.  But since the warning is potentially useful elsewhere, I disable
//   it only for the scope of these exceptions.

#	pragma warning(push)
#	pragma warning(disable:4673)
#endif

#ifdef _MSC_VER
#	pragma warning(pop)
#endif

typedef void CALLBACK FnType_SetDir( const char* dir );

class SaveStateBase;
class SysMtgsThread;

// --------------------------------------------------------------------------------------
//  SysCorePlugins Class
// --------------------------------------------------------------------------------------
//

class SysCorePlugins
{
	DeclareNoncopyableObject( SysCorePlugins );

protected:
	class PluginStatus_t
	{
	public:
		bool		IsInitialized;
		bool		IsOpened;
	public:
		PluginStatus_t()
		{
			IsInitialized	= false;
			IsOpened	= false;
		}

		virtual ~PluginStatus_t() { }
	};

public:		// hack until we unsuck plugins...
	std::unique_ptr<PluginStatus_t>	m_info;

public:
	SysCorePlugins();
	virtual ~SysCorePlugins();

	virtual void Load(  );
	virtual void Unload();

	bool AreLoaded() const;
	
	virtual bool Init();
	virtual bool Shutdown();
	virtual void Open();
	virtual void Close();

	virtual bool IsOpen( ) const;
	virtual bool IsInitialized( ) const;
	virtual bool IsLoaded( ) const;
protected:
	virtual bool NeedsClose() const;
	virtual bool NeedsOpen() const;

	virtual bool NeedsShutdown() const;
	virtual bool NeedsInit() const;
	
	virtual bool NeedsLoad() const;
	virtual bool NeedsUnload() const;

	friend class SysMtgsThread;
};

// GetPluginManager() is a required external implementation. This function is *NOT*
// provided by the PCSX2 core library.  It provides an interface for the linking User
// Interface apps or DLLs to reference their own instance of SysCorePlugins (also allowing
// them to extend the class and override virtual methods).

extern SysCorePlugins& GetCorePlugins();
