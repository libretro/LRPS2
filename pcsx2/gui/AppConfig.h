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

#include <memory>

#include "../Config.h"
#include "../PathDefs.h"
#include "../CDVD/CDVDaccess.h"

extern wxDirName		SettingsFolder;				// dictates where the settings folder comes from.

extern wxDirName GetCheatsFolder();
extern wxDirName GetCheatsWsFolder();

enum MemoryCardType
{
	MemoryCard_None,
	MemoryCard_File,
	MemoryCard_MaxCount
};

// =====================================================================================================
//  Pcsx2 Application Configuration. 
// =====================================================================================================

class AppConfig
{
public:
	// ------------------------------------------------------------------------
	struct FolderOptions
	{
		wxDirName
			Bios,
			Savestates,
			MemoryCards,
			Cheats,
			CheatsWS;

		wxString RunDisc; // last used location for Disc loading.

		void LoadSave();


		const wxDirName& operator[]( FoldersEnum_t folderidx ) const;
		wxDirName& operator[]( FoldersEnum_t folderidx );
	};

	// ------------------------------------------------------------------------
	struct FilenameOptions
	{
		wxFileName Bios;
	};

	// ------------------------------------------------------------------------
	// Options struct for each memory card.
	//
	struct McdOptions
	{
		wxFileName	Filename;	// user-configured location of this memory card
		bool		Enabled;	// memory card enabled (if false, memcard will not show up in-game)
		MemoryCardType Type;	// the memory card implementation that should be used
	};

public:
	// uses automatic ntfs compression when creating new memory cards (Win32 only)
#ifdef __WXMSW__
	bool		McdCompressNTFS;
#endif

	// Master toggle for enabling or disabling all speedhacks in one fail-free swoop.
	// (the toggle is applied when a new EmuConfig is sent through AppCoreThread::ApplySettings)
	bool		EnableSpeedHacks;
	bool		EnableGameFixes;

	// Presets try to prevent users from overwhelming when they want to change settings (usually to make a game run faster).
	// The presets allow to modify the balance between emulation accuracy and emulation speed using a pseudo-linear control.
	// It's pseudo since there's no way to arrange groups of all of pcsx2's settings such that each next group makes it slighty faster and slightly less compatiible for all games.
	//However, By carefully selecting these preset config groups, it's hopefully possible to achieve this goal for a reasonable percentage (hopefully above 50%) of the games.
	//when presets are enabled, the user has practically no control over the emulation settings, and can only choose the preset to use.

	// The next 2 vars enable/disable presets alltogether, and select/reflect current preset, respectively.
	bool					EnablePresets;
	int					PresetIndex;

	wxString				CurrentIso;
	wxString				CurrentELF;
	wxString				CurrentIRX;
	CDVD_SourceType				CdvdSource;
	wxString				CurrentGameArgs;

	// Memorycard options - first 2 are default slots, last 6 are multitap 1 and 2
	// slots (3 each)
	McdOptions				Mcd[8];
	wxString				GzipIsoIndexTemplate; // for quick-access index with gzipped ISO
	FolderOptions				Folders;
	FilenameOptions				BaseFilenames;
	
	// PCSX2-core emulation options, which are passed to the emu core prior to initiating
	// an emulation session.  Note these are the options saved into the GUI ini file and
	// which are shown as options in the gui preferences, but *not* necessarily the options
	// used by emulation.  The gui allows temporary per-game and commandline level overrides.
	Pcsx2Config				EmuOptions;

public:
	AppConfig();

	wxString FullpathToBios() const;
	wxString FullpathToMcd( uint slot ) const;

	void LoadSave();
	void LoadSaveRootItems();

	static int  GetMaxPresetIndex();
	
	bool IsOkApplyPreset(int n, bool ignoreMTVU);
	void ResetPresetSettingsToDefault();
};


extern void AppApplySettings();

extern void AppConfig_OnChangedSettingsFolder();

extern std::unique_ptr<AppConfig> g_Conf;
