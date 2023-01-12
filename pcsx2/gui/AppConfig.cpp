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

#include "App.h"

#include "MemoryCardFile.h"

#include <memory>

#include "options_tools.h"

wxDirName				CustomDocumentsFolder;
wxDirName				SettingsFolder;

//////////////////////////////////////////////////////////////////////////////////////////
// PathDefs Namespace -- contains default values for various pcsx2 path names and locations.
//
// Note: The members of this namespace are intended for default value initialization only.
// Most of the time you should use the path folder assignments in Conf() instead, since those
// are user-configurable.
//

namespace PathDefs
{
	namespace Base
	{
		const wxDirName& Savestates()
		{
			static const wxDirName retval( L"sstates" );
			return retval;
		}

		const wxDirName& MemoryCards()
		{
			static const wxDirName retval( L"memcards" );
			return retval;
		}

		const wxDirName& Settings()
		{
			static const wxDirName retval( L"inis" );
			return retval;
		}

		const wxDirName& Bios()
		{
			static const wxDirName retval(L"bios");
			return retval;
		}

		const wxDirName& Cheats()
		{
			static const wxDirName retval(L"cheats");
			return retval;
		}

		const wxDirName& CheatsWS()
		{
			static const wxDirName retval(L"cheats_ws");
			return retval;
		}
	};

	const wxDirName& LibretroPcsx2Root()
	{
		wxFileName dir(retroarch_system_path, "");
		dir.AppendDir("pcsx2");
		static const wxDirName dir_system_pcsx2(dir.GetPath());
		return dir_system_pcsx2;
	}


	wxDirName GetBios()
	{
		return LibretroPcsx2Root() + Base::Bios();
	}

	wxDirName GetCheats()
	{
		return LibretroPcsx2Root() + Base::Cheats();
	}

	wxDirName GetCheatsWS()
	{

		return LibretroPcsx2Root() + Base::CheatsWS();
	}
	
	wxDirName GetSavestates()
	{
		return LibretroPcsx2Root() + Base::Savestates();
	}

	wxDirName GetMemoryCards()
	{
		return LibretroPcsx2Root() + Base::MemoryCards();
	}

	wxDirName GetSettings()
	{
		return LibretroPcsx2Root() + Base::Settings();
	}

	wxDirName Get( FoldersEnum_t folderidx )
	{
		switch( folderidx )
		{
			case FolderId_Settings:		return GetSettings();
			case FolderId_Bios:			return GetBios();
			case FolderId_Savestates:	return GetSavestates();
			case FolderId_MemoryCards:	return GetMemoryCards();
			case FolderId_Cheats:		return GetCheats();
			case FolderId_CheatsWS:		return GetCheatsWS();

			case FolderId_Documents:	return CustomDocumentsFolder;

			default:
							break;
		}
		return wxDirName();
	}
};

wxDirName& AppConfig::FolderOptions::operator[]( FoldersEnum_t folderidx )
{
	switch( folderidx )
	{
		case FolderId_Settings:		return SettingsFolder;
		case FolderId_Bios:			return Bios;
		case FolderId_Savestates:	return Savestates;
		case FolderId_MemoryCards:	return MemoryCards;
		case FolderId_Cheats:		return Cheats;
		case FolderId_CheatsWS:		return CheatsWS;

		case FolderId_Documents:	return CustomDocumentsFolder;

		default:
						break;
	}
	return CustomDocumentsFolder;		// unreachable, but suppresses warnings.
}

const wxDirName& AppConfig::FolderOptions::operator[]( FoldersEnum_t folderidx ) const
{
	return const_cast<FolderOptions*>( this )->operator[]( folderidx );
}

static wxDirName GetResolvedFolder(FoldersEnum_t id)
{
	return  PathDefs::Get(id);
}


wxDirName GetCheatsFolder()
{
	return GetResolvedFolder(FolderId_Cheats);
}

wxDirName GetCheatsWsFolder()
{
	return GetResolvedFolder(FolderId_CheatsWS);
}

wxString AppConfig::FullpathToBios() const { return Path::Combine(Folders.Bios, BaseFilenames.Bios); }
wxString AppConfig::FullpathToMcd( uint slot ) const
{
	return Mcd[slot].Filename.GetFullPath();
}

AppConfig::AppConfig()
{
	#ifdef __WXMSW__
	McdCompressNTFS		= true;
	#endif
	EnableSpeedHacks	= true;
	EnableGameFixes		= false;

	EnablePresets		= true;
	PresetIndex		= 1;

	CdvdSource		= CDVD_SourceType::Iso;

	// To be moved to FileMemoryCard pluign (someday)
	for( uint slot=0; slot<8; ++slot )
	{
		Mcd[slot].Enabled	= !FileMcd_IsMultitapSlot(slot);	// enables main 2 slots
		Mcd[slot].Filename	= FileMcd_GetDefaultName( slot );

		// Folder memory card is autodetected later.
		Mcd[slot].Type = MemoryCardType::MemoryCard_File;
	}

	GzipIsoIndexTemplate = L"$(f).pindex.tmp";
}


void AppConfig::LoadSaveRootItems()
{

	wxFileName res(CurrentIso);
	CurrentIso = res.GetFullPath();
}

// ------------------------------------------------------------------------
void AppConfig::LoadSave()
{
	LoadSaveRootItems();

	// Process various sub-components:
	Folders			.LoadSave();
}


void AppConfig::FolderOptions::LoadSave()
{

	Savestates = PathDefs::GetSavestates();
	MemoryCards = PathDefs::GetMemoryCards();
	Cheats = PathDefs::GetCheats();
	CheatsWS = PathDefs::GetCheatsWS();

	for( int i=0; i<FolderId_COUNT; ++i )
		operator[]( (FoldersEnum_t)i ).Normalize();
}

// ----------------------------------------------------------------------------
int AppConfig::GetMaxPresetIndex()
{
	return 5;
}

// Apply one of several (currently 6) configuration subsets.
// The scope of the subset which each preset controlls is hardcoded here.
// Use ignoreMTVU to avoid updating the MTVU field.
// Main purpose is for the preset enforcement at launch, to avoid overwriting a user's setting.
bool AppConfig::IsOkApplyPreset(int n, bool ignoreMTVU)
{
	if (n < 0 || n > GetMaxPresetIndex() )
	{
		log_cb(RETRO_LOG_INFO, "DEV Warning: ApplyPreset(%i): index out of range, Aborting.\n", n);
		return false;
	}

	log_cb(RETRO_LOG_INFO, "Applying preset n: %i\n", n);

	//Have some original and default values at hand to be used later.
	Pcsx2Config::GSOptions        original_GS = EmuOptions.GS;
	Pcsx2Config::SpeedhackOptions original_SpeedHacks = EmuOptions.Speedhacks;
	AppConfig				default_AppConfig;
	Pcsx2Config				default_Pcsx2Config;

	//  NOTE:	Because the system currently only supports passing of an entire AppConfig to the GUI panels/menus to apply/reflect,
	//			the GUI entities should be aware of the settings which the presets control, such that when presets are used:
	//			1. The panels/entities should prevent manual modifications (by graying out) of settings which the presets control.
	//			2. The panels should not apply values which the presets don't control if the value is initiated by a preset.
	//			Currently controlled by the presets:
	//			- AppConfig:	Framerate (except turbo/slowmo factors), EnableSpeedHacks, EnableGameFixes.
	//			- EmuOptions:	Cpu, Gamefixes, SpeedHacks (except mtvu), EnablePatches, GS (except for FrameLimitEnable and VsyncEnable).
	//
	//			This essentially currently covers all the options on all the panels except for framelimiter which isn't
	//			controlled by the presets, and the entire GSWindow panel which also isn't controlled by presets
	//
	//			So, if changing the scope of the presets (making them affect more or less values), the relevant GUI entities
	//			should me modified to support it.


	//Force some settings as a (current) base for all presets.
	EnableGameFixes		= false;

	EmuOptions.EnablePatches		= true;
	EmuOptions.GS					= default_Pcsx2Config.GS;
	EmuOptions.GS.VsyncQueueSize	= original_GS.VsyncQueueSize;
	EmuOptions.Cpu					= default_Pcsx2Config.Cpu;
	EmuOptions.Gamefixes			= default_Pcsx2Config.Gamefixes;
	EmuOptions.Speedhacks			= default_Pcsx2Config.Speedhacks;
	EmuOptions.Speedhacks.bitset	= 0; //Turn off individual hacks to make it visually clear they're not used.
	EmuOptions.Speedhacks.vuThread	= original_SpeedHacks.vuThread;
	EmuOptions.Speedhacks.vu1Instant = original_SpeedHacks.vu1Instant;
	EnableSpeedHacks = true;
	// Actual application of current preset over the base settings which all presets use (mostly pcsx2's default values).

	bool isRateSet = false, isSkipSet = false, isMTVUSet = ignoreMTVU ? true : false; // used to prevent application of specific lower preset values on fallthrough.
	switch (n) // Settings will waterfall down to the Safe preset, then stop. So, Balanced and higher will inherit any settings through Safe.
	{
		case 5: // Mostly Harmful
			isRateSet ? 0 : (isRateSet = true, EmuOptions.Speedhacks.EECycleRate = 1); // +1 EE cyclerate
			isSkipSet ? 0 : (isSkipSet = true, EmuOptions.Speedhacks.EECycleSkip = 1); // +1 EE cycle skip
            // Fall through

		case 4: // Very Aggressive
			isRateSet ? 0 : (isRateSet = true, EmuOptions.Speedhacks.EECycleRate = -2); // -2 EE cyclerate
            // Fall through

		case 3: // Aggressive
			isRateSet ? 0 : (isRateSet = true, EmuOptions.Speedhacks.EECycleRate = -1); // -1 EE cyclerate
            // Fall through

		case 2: // Balanced
			isMTVUSet ? 0 : (isMTVUSet = true, EmuOptions.Speedhacks.vuThread = true); // Enable MTVU
            // Fall through

		case 1: // Safe (Default)
			EmuOptions.Speedhacks.IntcStat = true;
			EmuOptions.Speedhacks.WaitLoop = true;
			EmuOptions.Speedhacks.vuFlagHack = true;
			EmuOptions.Speedhacks.vu1Instant = true;

			// If waterfalling from > Safe, break to avoid MTVU disable.
			if (n > 1) break;
            // Fall through
			
		case 0: // Safest
			if (n == 0) EmuOptions.Speedhacks.vu1Instant = false;
			isMTVUSet ? 0 : (isMTVUSet = true, EmuOptions.Speedhacks.vuThread = false); // Disable MTVU
			break;

		default:
			log_cb(RETRO_LOG_WARN, "Developer Warning: Preset #%d is not implemented. (--> Using application default).\n", n);
	}


	EnablePresets=true;
	PresetIndex=n;

	return true;
}

void AppConfig::ResetPresetSettingsToDefault() {
	EmuOptions.Speedhacks.EECycleRate = 0;
	EmuOptions.Speedhacks.EECycleSkip = 0;
	EmuOptions.Speedhacks.vuThread = false;
	EmuOptions.Speedhacks.vu1Instant = true;
	EmuOptions.Speedhacks.IntcStat = true;
	EmuOptions.Speedhacks.WaitLoop = true;
	EmuOptions.Speedhacks.vuFlagHack = true;
	PresetIndex = 1;
	log_cb(RETRO_LOG_INFO, "Reset of speedhack preset to n:1 - Safe (default)\n");
}


static void LoadUiSettings()
{
	g_Conf = std::make_unique<AppConfig>();
	g_Conf->LoadSave();

	if( !wxFile::Exists( g_Conf->CurrentIso ) )
		g_Conf->CurrentIso.clear();

	sApp.DispatchUiSettingsEvent();
}

static void LoadVmSettings()
{
	// Load virtual machine options and apply some defaults overtop saved items, which
	// are regulated by the PCSX2 UI.

	g_Conf->EnablePresets = true;
	g_Conf->PresetIndex = option_value(INT_PCSX2_OPT_SPEEDHACKS_PRESET, KeyOptionInt::return_type);
	

	if (g_Conf->EnablePresets) 
		g_Conf->IsOkApplyPreset(g_Conf->PresetIndex, false);
	else
		g_Conf->ResetPresetSettingsToDefault();

	sApp.DispatchVmSettingsEvent();
}

static void AppLoadSettings()
{
	if(wxGetApp().Rpc_TryInvoke(AppLoadSettings))
		return;

	LoadUiSettings();
	LoadVmSettings();
}

// Parameters:
//   overwrite - this option forces the current settings to overwrite any existing settings
//      that might be saved to the configured ini/settings folder.
//
// Notes:
//   The overwrite option applies to PCSX2 options only.
//
void AppConfig_OnChangedSettingsFolder()
{
	AppLoadSettings();
	AppApplySettings();

}
