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

#include "MemoryCardFile.h"
#include "Utilities/IniInterface.h"

#include <wx/stdpaths.h>
#include <memory>

#include "options_tools.h"

// --------------------------------------------------------------------------------------
//  pxDudConfig
// --------------------------------------------------------------------------------------
// Used to handle config actions prior to the creation of the ini file (for example, the
// first time wizard).  Attempts to save ini settings are simply ignored through this
// class, which allows us to give the user a way to set everything up in the wizard, apply
// settings as usual, and only *save* something once the whole wizard is complete.
//
class pxDudConfig : public wxConfigBase
{
protected:
	wxString	m_empty;

public:
	virtual ~pxDudConfig() = default;

	virtual void SetPath(const wxString& ) {}
	virtual const wxString& GetPath() const { return m_empty; }

	virtual bool GetFirstGroup(wxString& , long& ) const { return false; }
	virtual bool GetNextGroup (wxString& , long& ) const { return false; }
	virtual bool GetFirstEntry(wxString& , long& ) const { return false; }
	virtual bool GetNextEntry (wxString& , long& ) const { return false; }
	virtual size_t GetNumberOfEntries(bool ) const  { return 0; }
	virtual size_t GetNumberOfGroups(bool ) const  { return 0; }

	virtual bool HasGroup(const wxString& ) const { return false; }
	virtual bool HasEntry(const wxString& ) const { return false; }

	virtual bool Flush(bool ) { return false; }

	virtual bool RenameEntry(const wxString&, const wxString& ) { return false; }

	virtual bool RenameGroup(const wxString&, const wxString& ) { return false; }

	virtual bool DeleteEntry(const wxString&, bool bDeleteGroupIfEmpty = true) { return false; }
	virtual bool DeleteGroup(const wxString& ) { return false; }
	virtual bool DeleteAll() { return false; }

protected:
	virtual bool DoReadString(const wxString& , wxString *) const  { return false; }
	virtual bool DoReadLong(const wxString& , long *) const  { return false; }

	virtual bool DoWriteString(const wxString& , const wxString& )  { return false; }
	virtual bool DoWriteLong(const wxString& , long )  { return false; }
};


DocsModeType				DocsFolderMode = DocsFolder_User;
bool					UseDefaultSettingsFolder = true;
bool					UseDefaultPluginsFolder = true;


wxDirName				CustomDocumentsFolder;
wxDirName				SettingsFolder;

wxDirName				PluginsFolder;

static pxDudConfig _dud_config;

// Returns the current application configuration file.  
// This is preferred over using
// wxConfigBase::GetAppConfig(), since it defaults to 
// *not* creating a config file
// automatically (which is typically highly undesired 
// behavior in our system)
static wxConfigBase* GetAppConfig(void)
{
	return wxConfigBase::Get( false );
}

// --------------------------------------------------------------------------------------
//  AppIniSaver / AppIniLoader
// --------------------------------------------------------------------------------------
class AppIniSaver : public IniSaver
{
public:
	AppIniSaver();
	virtual ~AppIniSaver() = default;
};

class AppIniLoader : public IniLoader
{
public:
	AppIniLoader();
	virtual ~AppIniLoader() = default;
};

AppIniSaver::AppIniSaver()
	: IniSaver( (GetAppConfig() != NULL) ? *GetAppConfig() : _dud_config )
{
}

AppIniLoader::AppIniLoader()
	: IniLoader( (GetAppConfig() != NULL) ? *GetAppConfig() : _dud_config )
{
}

//////////////////////////////////////////////////////////////////////////////////////////
// PathDefs Namespace -- contains default values for various pcsx2 path names and locations.
//
// Note: The members of this namespace are intended for default value initialization only.
// Most of the time you should use the path folder assignments in Conf() instead, since those
// are user-configurable.
//

// Specifies the main configuration folder.
static wxDirName GetUserLocalDataDir()
{
	return wxDirName(wxStandardPaths::Get().GetUserLocalDataDir());
}

// Fetches the path location for user-consumable documents -- stuff users are likely to want to
// share with other programs: screenshots, memory cards, and savestates.
static wxDirName GetDocuments( DocsModeType mode )
{
	switch( mode )
	{
#ifdef XDG_STD
		// Move all user data file into central configuration directory (XDG_CONFIG_DIR)
		case DocsFolder_User:	return GetUserLocalDataDir();
#else
		case DocsFolder_User:	return (wxDirName)Path::Combine( wxStandardPaths::Get().GetDocumentsDir(), L"PCSX2" );
#endif
		case DocsFolder_Custom: return CustomDocumentsFolder;

					jNO_DEFAULT
	}

	return wxDirName();
}

static wxDirName GetDocuments()
{
	return GetDocuments( DocsFolderMode );
}

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

		const wxDirName& Plugins()
		{
			static const wxDirName retval( L"plugins" );
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

	// Specifies the root folder for the application install.
	// (currently it's the CWD, but in the future I intend to move all binaries to a "bin"
	// sub folder, in which case the approot will become "..") [- Air?]

	//The installer installs the folders which are relative to AppRoot (that's plugins/langs)
	//  relative to the exe folder, and not relative to cwd. So the exe should be default AppRoot. - avih
	const wxDirName& AppRoot()
	{
		static const wxDirName appCache( (wxDirName)
				wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath() );
		return appCache;
	}



	wxDirName GetBios()
	{
		return GetDocuments() + Base::Bios();;
	}

	wxDirName GetCheats()
	{
		return GetDocuments() + Base::Cheats();
	}

	wxDirName GetCheatsWS()
	{
		return GetDocuments() + Base::CheatsWS();
	}
	
	wxDirName GetSavestates()
	{
		return GetDocuments() + Base::Savestates();
	}

	wxDirName GetMemoryCards()
	{
		return GetDocuments() + Base::MemoryCards();
	}

	wxDirName GetPlugins()
	{
		// Each linux distributions have his rules for path so we give them the possibility to
		// change it with compilation flags. -- Gregory
#ifndef PLUGIN_DIR_COMPILATION
		return AppRoot() + Base::Plugins();
#else
#define xPLUGIN_DIR_str(s) PLUGIN_DIR_str(s)
#define PLUGIN_DIR_str(s) #s
		return wxDirName( xPLUGIN_DIR_str(PLUGIN_DIR_COMPILATION) );
#endif
	}

	wxDirName GetSettings()
	{
		return GetDocuments() + Base::Settings();
	}

	wxDirName Get( FoldersEnum_t folderidx )
	{
		switch( folderidx )
		{
			case FolderId_Plugins:		return GetPlugins();
			case FolderId_Settings:		return GetSettings();
			case FolderId_Bios:			return GetBios();
			case FolderId_Savestates:	return GetSavestates();
			case FolderId_MemoryCards:	return GetMemoryCards();
			case FolderId_Cheats:		return GetCheats();
			case FolderId_CheatsWS:		return GetCheatsWS();

			case FolderId_Documents:	return CustomDocumentsFolder;

			jNO_DEFAULT
		}
		return wxDirName();
	}
};

wxDirName& AppConfig::FolderOptions::operator[]( FoldersEnum_t folderidx )
{
	switch( folderidx )
	{
		case FolderId_Plugins:		return PluginsFolder;
		case FolderId_Settings:		return SettingsFolder;
		case FolderId_Bios:			return Bios;
		case FolderId_Savestates:	return Savestates;
		case FolderId_MemoryCards:	return MemoryCards;
		case FolderId_Cheats:		return Cheats;
		case FolderId_CheatsWS:		return CheatsWS;

		case FolderId_Documents:	return CustomDocumentsFolder;

		jNO_DEFAULT
	}
	return PluginsFolder;		// unreachable, but suppresses warnings.
}

const wxDirName& AppConfig::FolderOptions::operator[]( FoldersEnum_t folderidx ) const
{
	return const_cast<FolderOptions*>( this )->operator[]( folderidx );
}

bool AppConfig::FolderOptions::IsDefault( FoldersEnum_t folderidx ) const
{
	switch( folderidx )
	{
		case FolderId_Plugins:		return UseDefaultPluginsFolder;
		case FolderId_Settings:		return UseDefaultSettingsFolder;
		case FolderId_Bios:			return UseDefaultBios;
		case FolderId_Savestates:	return UseDefaultSavestates;
		case FolderId_MemoryCards:	return UseDefaultMemoryCards;
		case FolderId_Cheats:		return UseDefaultCheats;
		case FolderId_CheatsWS:		return UseDefaultCheatsWS;

		case FolderId_Documents:	return false;

		jNO_DEFAULT
	}
	return false;
}

void AppConfig::FolderOptions::Set( FoldersEnum_t folderidx, const wxString& src, bool useDefault )
{
	switch( folderidx )
	{
		case FolderId_Plugins:
			PluginsFolder = src;
			UseDefaultPluginsFolder = useDefault;
		break;

		case FolderId_Settings:
			SettingsFolder = src;
			UseDefaultSettingsFolder = useDefault;
		break;

		case FolderId_Bios:
			Bios = src;
			UseDefaultBios = useDefault;
		break;

		case FolderId_Savestates:
			Savestates = src;
			UseDefaultSavestates = useDefault;
		break;

		case FolderId_MemoryCards:
			MemoryCards = src;
			UseDefaultMemoryCards = useDefault;
		break;

		case FolderId_Documents:
			CustomDocumentsFolder = src;
		break;

		case FolderId_Cheats:
			Cheats = src;
			UseDefaultCheats = useDefault;
		break;

		case FolderId_CheatsWS:
			CheatsWS = src;
			UseDefaultCheatsWS = useDefault;
		break;

		jNO_DEFAULT
	}
}

// --------------------------------------------------------------------------------------
//  Default Filenames
// --------------------------------------------------------------------------------------
namespace FilenameDefs
{
	wxFileName GetUiConfig()
	{
		return wxFileName(L"PCSX2_ui.ini");
	}

	wxFileName GetVmConfig()
	{
		return wxFileName(L"PCSX2_vm.ini");
	}

	const wxFileName& Memcard( uint port, uint slot )
	{
		static const wxFileName retval[2][4] =
		{
			{
				wxFileName( L"Mcd001.ps2" ),
				wxFileName( L"Mcd003.ps2" ),
				wxFileName( L"Mcd005.ps2" ),
				wxFileName( L"Mcd007.ps2" ),
			},
			{
				wxFileName( L"Mcd002.ps2" ),
				wxFileName( L"Mcd004.ps2" ),
				wxFileName( L"Mcd006.ps2" ),
				wxFileName( L"Mcd008.ps2" ),
			}
		};

		IndexBoundsAssumeDev( L"FilenameDefs::Memcard", port, 2 );
		IndexBoundsAssumeDev( L"FilenameDefs::Memcard", slot, 4 );

		return retval[port][slot];
	}
};

static wxDirName GetResolvedFolder(FoldersEnum_t id)
{
	return g_Conf->Folders.IsDefault(id) ? PathDefs::Get(id) : g_Conf->Folders[id];
}


wxDirName GetCheatsFolder()
{
	return GetResolvedFolder(FolderId_Cheats);
}

wxDirName GetCheatsWsFolder()
{
	return GetResolvedFolder(FolderId_CheatsWS);
}

wxDirName GetSettingsFolder()
{
	if( wxGetApp().Overrides.SettingsFolder.IsOk() )
		return wxGetApp().Overrides.SettingsFolder;

	return UseDefaultSettingsFolder ? PathDefs::GetSettings() : SettingsFolder;
}

static wxString GetVmSettingsFilename()
{
	wxFileName fname( wxGetApp().Overrides.VmSettingsFile.IsOk() ? wxGetApp().Overrides.VmSettingsFile : FilenameDefs::GetVmConfig() );
	return GetSettingsFolder().Combine( fname ).GetFullPath();
}

static wxString GetUiSettingsFilename()
{
	wxFileName fname( FilenameDefs::GetUiConfig() );
	return GetSettingsFolder().Combine( fname ).GetFullPath();
}


wxString AppConfig::FullpathToBios() const				{ return Path::Combine( Folders.Bios, BaseFilenames.Bios ); }
wxString AppConfig::FullpathToMcd( uint slot ) const
{
	if (Mcd[slot].Type == MemoryCardType::MemoryCard_File)
		return Mcd[slot].Filename.GetFullPath();
	return Mcd[slot].Filename.GetPath();
}

AppConfig::AppConfig()
{
	#ifdef __WXMSW__
	McdCompressNTFS		= true;
	#endif
	EnableSpeedHacks	= true;
	EnableGameFixes		= false;
	EnableFastBoot		= true;

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

// ------------------------------------------------------------------------
void AppConfig::LoadSaveMemcards( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"MemoryCards" );

	for( uint slot=0; slot<2; ++slot )
	{
		ini.Entry( pxsFmt( L"Slot%u_Enable", slot+1 ),
			Mcd[slot].Enabled, Mcd[slot].Enabled );
		ini.Entry( pxsFmt( L"Slot%u_Filename", slot+1 ),
			Mcd[slot].Filename, Mcd[slot].Filename );
	}

	for( uint slot=2; slot<8; ++slot )
	{
		int mtport = FileMcd_GetMtapPort(slot)+1;
		int mtslot = FileMcd_GetMtapSlot(slot)+1;

		ini.Entry( pxsFmt( L"Multitap%u_Slot%u_Enable", mtport, mtslot ),
			Mcd[slot].Enabled, Mcd[slot].Enabled );
		ini.Entry( pxsFmt( L"Multitap%u_Slot%u_Filename", mtport, mtslot ),
			Mcd[slot].Filename, Mcd[slot].Filename );
	}
}

void AppConfig::LoadSaveRootItems( IniInterface& ini )
{
	IniEntry( GzipIsoIndexTemplate );

	wxFileName res(CurrentIso);
	CurrentIso = res.GetFullPath();

	IniEntry( CurrentBlockdump );
	IniEntry( CurrentELF );
	IniEntry( CurrentIRX );

	IniEntry( EnableSpeedHacks );
	IniEntry( EnableGameFixes );
	IniEntry( EnableFastBoot );

	IniEntry( EnablePresets );
	IniEntry( PresetIndex );
	
	#ifdef __WXMSW__
	IniEntry( McdCompressNTFS );
	#endif
}

// ------------------------------------------------------------------------
void AppConfig::LoadSave( IniInterface& ini )
{
	LoadSaveRootItems( ini );
	LoadSaveMemcards( ini );

	// Process various sub-components:
	Folders			.LoadSave( ini );
	BaseFilenames	.LoadSave( ini );

	ini.Flush();
}
void AppConfig::FolderOptions::ApplyDefaults()
{
	if( UseDefaultBios )		Bios		  = PathDefs::GetBios();
	if( UseDefaultSavestates )	Savestates	  = PathDefs::GetSavestates();
	if( UseDefaultMemoryCards )	MemoryCards	  = PathDefs::GetMemoryCards();
	if( UseDefaultPluginsFolder)PluginsFolder = PathDefs::GetPlugins();
	if( UseDefaultCheats )      Cheats		  = PathDefs::GetCheats();
	if( UseDefaultCheatsWS )    CheatsWS	  = PathDefs::GetCheatsWS();
}

// ------------------------------------------------------------------------
AppConfig::FolderOptions::FolderOptions()
	: Bios			( PathDefs::GetBios() )
	, Savestates	( PathDefs::GetSavestates() )
	, MemoryCards	( PathDefs::GetMemoryCards() )
	, Cheats		( PathDefs::GetCheats() )
	, CheatsWS      ( PathDefs::GetCheatsWS() )
	, RunDisc	( GetDocuments().GetFilename() )
{
	bitset = 0xffffffff;
}

void AppConfig::FolderOptions::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"Folders" );

	if( ini.IsSaving() )
	{
		ApplyDefaults();
	}

	IniBitBool( UseDefaultBios );
	IniBitBool( UseDefaultSavestates );
	IniBitBool( UseDefaultMemoryCards );
	IniBitBool( UseDefaultPluginsFolder );
	IniBitBool( UseDefaultCheats );
	IniBitBool( UseDefaultCheatsWS );

	//when saving in portable mode, we save relative paths if possible
	 //  --> on load, these relative paths will be expanded relative to the exe folder.
	bool rel = ( ini.IsLoading() );
	
	IniEntryDirFile( Bios,  rel);
	IniEntryDirFile( Savestates,  rel );
	IniEntryDirFile( MemoryCards,  rel );
	IniEntryDirFile( Cheats, rel );
	IniEntryDirFile( CheatsWS, rel );

	IniEntryDirFile( RunDisc, rel );

	if( ini.IsLoading() )
	{
		ApplyDefaults();

		for( int i=0; i<FolderId_COUNT; ++i )
			operator[]( (FoldersEnum_t)i ).Normalize();
	}
}

// ------------------------------------------------------------------------
const wxFileName& AppConfig::FilenameOptions::operator[]( PluginsEnum_t pluginidx ) const
{
	IndexBoundsAssumeDev( L"Filename[Plugin]", pluginidx, PluginId_Count );
	return Plugins[pluginidx];
}

void AppConfig::FilenameOptions::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"Filenames" );

	static const wxFileName pc( L"Please Configure" );

	//when saving in portable mode, we just save the non-full-path filename
 	//  --> on load they'll be initialized with default (relative) paths (works both for plugins and bios)
	//note: this will break if converting from install to portable, and custom folders are used. We can live with that.
	for( int i=0; i<PluginId_Count; ++i )
	{
		ini.Entry( tbl_PluginInfo[i].GetShortname(), Plugins[i], pc );
	}

	ini.Entry( L"BIOS", Bios, pc );
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



static wxFileConfig* OpenFileConfig( const wxString& filename )
{
	return new wxFileConfig( wxEmptyString, wxEmptyString, filename, wxEmptyString, wxCONFIG_USE_RELATIVE_PATH );
}

static void SaveUiSettings()
{
	if( !wxFile::Exists( g_Conf->CurrentIso ) )
		g_Conf->CurrentIso.clear();

#if defined(_WIN32)
	if (!g_Conf->Folders.RunDisc.DirExists())
		g_Conf->Folders.RunDisc.Clear();
#else
	if (!g_Conf->Folders.RunDisc.Exists())
		g_Conf->Folders.RunDisc.Clear();
#endif
	AppIniSaver saver;
	g_Conf->LoadSave( saver );
	sApp.DispatchUiSettingsEvent( saver );
}

static void SaveVmSettings()
{
	std::unique_ptr<wxFileConfig> vmini( OpenFileConfig( GetVmSettingsFilename() ) );
	IniSaver vmsaver( vmini.get() );
	g_Conf->EmuOptions.LoadSave( vmsaver );

	sApp.DispatchVmSettingsEvent( vmsaver );
}

void AppSaveSettings(void)
{
	// If multiple SaveSettings messages are requested, we want to ignore most of them.
	// Saving settings once when the GUI is idle should be fine. :)

	static std::atomic<bool> isPosted(false);

	if( !wxThread::IsMain() )
		return;

	//log_cb(RETRO_LOG_INFO, "Saving ini files...\n");

	SaveUiSettings();
	SaveVmSettings();

	isPosted = false;
}

static void LoadUiSettings()
{
	AppIniLoader loader;
	g_Conf = std::make_unique<AppConfig>();
	g_Conf->LoadSave( loader );

	if( !wxFile::Exists( g_Conf->CurrentIso ) )
		g_Conf->CurrentIso.clear();

#if defined(_WIN32)
	if( !g_Conf->Folders.RunDisc.DirExists() )
		g_Conf->Folders.RunDisc.Clear();
#else
	if (!g_Conf->Folders.RunDisc.Exists())
		g_Conf->Folders.RunDisc.Clear();
#endif

	sApp.DispatchUiSettingsEvent( loader );
}

static void LoadVmSettings()
{
	// Load virtual machine options and apply some defaults overtop saved items, which
	// are regulated by the PCSX2 UI.

	std::unique_ptr<wxFileConfig> vmini( OpenFileConfig( GetVmSettingsFilename() ) );
	IniLoader vmloader( vmini.get() );
	g_Conf->EmuOptions.LoadSave( vmloader );

	g_Conf->EnablePresets = true;
	g_Conf->PresetIndex = option_value(INT_PCSX2_OPT_SPEEDHACKS_PRESET, KeyOptionInt::return_type);
	

	if (g_Conf->EnablePresets) 
		g_Conf->IsOkApplyPreset(g_Conf->PresetIndex, false);
	else
		g_Conf->ResetPresetSettingsToDefault();

	sApp.DispatchVmSettingsEvent( vmloader );
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
//   The overwrite option applies to PCSX2 options only.  Plugin option behavior will depend
//   on the plugins.
//
void AppConfig_OnChangedSettingsFolder()
{
	GetDocuments().Mkdir();
	GetSettingsFolder().Mkdir();

	const wxString iniFilename( GetUiSettingsFilename() );

	// Bind into wxConfigBase to allow wx to use our config internally, and delete whatever
	// comes out (cleans up prev config, if one).
	delete wxConfigBase::Set( OpenFileConfig( iniFilename ) );
	GetAppConfig()->SetRecordDefaults(true);

	AppLoadSettings();
	AppApplySettings();
	AppSaveSettings();//Make sure both ini files are created if needed.
}
