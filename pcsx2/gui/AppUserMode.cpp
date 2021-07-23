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
#include "Utilities/IniInterface.h"

#include <wx/stdpaths.h>

#ifdef __WXMSW__
#include "wx/msw/regconf.h"
#endif

DocsModeType				DocsFolderMode = DocsFolder_User;
bool					UseDefaultSettingsFolder = true;
bool					UseDefaultPluginsFolder = true;


wxDirName				CustomDocumentsFolder;
wxDirName				SettingsFolder;

wxDirName				InstallFolder;
wxDirName				PluginsFolder;

wxConfigBase* Pcsx2App::OpenInstallSettingsFile()
{
	// Implementation Notes:
	//
	// As of 0.9.8 and beyond, PCSX2's versioning should be strong enough to base ini and
	// plugin compatibility on version information alone.  This in turn allows us to ditch
	// the old system (CWD-based ini file mess) in favor of a system that simply stores
	// most core application-level settings in the registry.

	std::unique_ptr<wxConfigBase> conf_install;

#ifdef __WXMSW__
	conf_install = std::unique_ptr<wxConfigBase>(new wxRegConfig());
#else
	// FIXME!!  Linux / Mac
	// Where the heck should this information be stored?

	wxDirName usrlocaldir( PathDefs::GetUserLocalDataDir() );
	//wxDirName usrlocaldir( wxStandardPaths::Get().GetDataDir() );
	if( !usrlocaldir.Exists() )
	{
		log_cb(RETRO_LOG_INFO, "Creating UserLocalData folder: %s\n", usrlocaldir.ToString().c_str());
		usrlocaldir.Mkdir();
	}

	wxFileName usermodefile( GetAppName() + L"-reg.ini" );
	usermodefile.SetPath( usrlocaldir.ToString() );
	conf_install = std::unique_ptr<wxConfigBase>(OpenFileConfig( usermodefile.GetFullPath() ));
#endif

	return conf_install.release();
}


void Pcsx2App::EstablishAppUserMode()
{
	std::unique_ptr<wxConfigBase> conf_install = std::unique_ptr<wxConfigBase>(OpenInstallSettingsFile());

	conf_install->SetRecordDefaults(false);

	App_LoadInstallSettings( conf_install.get() );
	AppConfig_OnChangedSettingsFolder( false );
}
