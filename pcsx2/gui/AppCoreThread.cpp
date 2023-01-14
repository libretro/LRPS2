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
#include "AppGameDatabase.h"

#include "Utilities/Threading.h"

#include "../ps2/BiosTools.h"
#include "../GS.h"

#include "../CDVD/CDVD.h"
#include "../Elfheader.h"
#include "../Patch.h"
#include "../R5900Exceptions.h"
#include "../Sio.h"

#include "retro_messager.h"

extern const wxChar* tbl_SpeedhackNames[];
extern const wxChar *tbl_GamefixNames[];

ImplementEnumOperators(SSE_RoundMode);

class RecursionGuard
{
public:
    int &Counter;

    RecursionGuard(int &counter)
        : Counter(counter)
    {
        ++Counter;
    }

    virtual ~RecursionGuard()
    {
        --Counter;
    }
};

/*
* This bool variable is used to keep trace if the messages to the frontend about "cheat ws found" is already sent.
* This is a side effect of a strange problem that at game boot the function _ApplySettings runs multiple time:
* this is to be investigate deeper, if it's the cause of the very long boot time when cheat ws are enabled
*/
bool msg_cheat_ws_found_sent = false;
bool msg_cheat_60fps_found_sent = false;
bool msg_cheat_nointerlacing_found_sent = false;
bool msg_cheats_found_sent   = false;

__aligned16 SysMtgsThread mtgsThread;
__aligned16 AppCoreThread CoreThread;

typedef void (AppCoreThread::*FnPtr_CoreThreadMethod)();

SysCoreThread& GetCoreThread()
{
	return CoreThread;
}

SysMtgsThread& GetMTGS()
{
	return mtgsThread;
}

static void PostCoreStatus(CoreThreadStatus pevt)
{
	sApp.PostAction(CoreThreadStatusEvent(pevt));
}

/*
* Resets stuffs related to content, 
* needed when closing and reopening a game
* * it's called from retro_load_game
*/
void ResetContentStuffs(void)
{
	msg_cheat_ws_found_sent            = false;
	msg_cheat_60fps_found_sent         = false;
	msg_cheat_nointerlacing_found_sent = false;
	msg_cheats_found_sent              = false;
	ElfCRC                             = 0;
}

// --------------------------------------------------------------------------------------
//  AppCoreThread Implementations
// --------------------------------------------------------------------------------------
AppCoreThread::AppCoreThread()
	: SysCoreThread()
{
	m_resetCdvd = false;
}

AppCoreThread::~AppCoreThread()
{
	m_resetCdvd = false;
	_parent::Cancel(wxTimeSpan(0, 0, 4, 0));
}

void AppCoreThread::Cancel()
{
	_parent::Cancel(wxTimeSpan(0, 0, 4, 0));
}

void AppCoreThread::Reset()
{
	_parent::Reset();
}

void AppCoreThread::ResetQuick()
{
	_parent::ResetQuick();
}

void AppCoreThread::Suspend()
{
	if (IsOpen())
		_parent::Suspend();
}

void AppCoreThread::Resume()
{
	_parent::Resume();
}

void Pcsx2App::SysApplySettings()
{
	if (AppRpc_TryInvoke(&Pcsx2App::SysApplySettings))
		return;
	CoreThread.ApplySettings(g_Conf->EmuOptions);

	CDVD_SourceType cdvdsrc(g_Conf->CdvdSource);
	if (cdvdsrc != CDVDsys_GetSourceType() || (cdvdsrc == CDVD_SourceType::Iso && (CDVDsys_GetFile(cdvdsrc) != g_Conf->CurrentIso)))
		CoreThread.ResetCdvd();

	CDVDsys_SetFile(CDVD_SourceType::Iso, g_Conf->CurrentIso);
}

void AppCoreThread::OnResumeReady()
{
	wxGetApp().SysApplySettings();
	_parent::OnResumeReady();
}

void AppCoreThread::OnPause()
{
	_parent::OnPause();
}

// Load Game Settings found in database
// (game fixes, round modes, clamp modes, etc...)
// Returns number of gamefixes set
static int loadGameSettings(Pcsx2Config& dest, const GameDatabaseSchema::GameEntry& game)
{
	if (!game.isValid)
		return 0;

	int gf = 0;

	if (game.eeRoundMode != GameDatabaseSchema::RoundMode::Undefined)
	{
		SSE_RoundMode eeRM = (SSE_RoundMode)enum_cast(game.eeRoundMode);
		if (((int)eeRM >= SSE_RoundMode_FIRST) && ((int)eeRM < SSE_RoundMode_COUNT))
		{
			dest.Cpu.sseMXCSR.SetRoundMode(eeRM);
			gf++;
		}
	}

	if (game.vuRoundMode != GameDatabaseSchema::RoundMode::Undefined)
	{
		SSE_RoundMode vuRM = (SSE_RoundMode)enum_cast(game.vuRoundMode);
		if (((int)vuRM >= SSE_RoundMode_FIRST) && ((int)vuRM < SSE_RoundMode_COUNT))
		{
			dest.Cpu.sseVUMXCSR.SetRoundMode(vuRM);
			gf++;
		}
	}

	if (game.eeClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		int clampMode = enum_cast(game.eeClampMode);
		log_cb(RETRO_LOG_INFO, "(GameDB) Changing EE/FPU clamp mode [mode=%d]\n", clampMode);
		dest.Cpu.Recompiler.fpuOverflow = (clampMode >= 1);
		dest.Cpu.Recompiler.fpuExtraOverflow = (clampMode >= 2);
		dest.Cpu.Recompiler.fpuFullMode = (clampMode >= 3);
		gf++;
	}

	if (game.vuClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		int clampMode = enum_cast(game.vuClampMode);
		log_cb(RETRO_LOG_INFO, "(GameDB) Changing VU0/VU1 clamp mode [mode=%d]\n", clampMode);
		dest.Cpu.Recompiler.vuOverflow = (clampMode >= 1);
		dest.Cpu.Recompiler.vuExtraOverflow = (clampMode >= 2);
		dest.Cpu.Recompiler.vuSignOverflow = (clampMode >= 3);
		gf++;
	}

	// TODO - config - this could be simplified with maps instead of bitfields and enums
	for (SpeedhackId id = SpeedhackId_FIRST; id < pxEnumEnd; id++)
	{
		const wxChar *str = tbl_SpeedhackNames[id];
		std::string key   = wxString(str).ToStdString() + "SpeedHack";

		// Gamefixes are already guaranteed to be valid, any invalid ones are dropped
		if (game.speedHacks.count(key) == 1)
		{
			// Legacy note - speedhacks are setup in the GameDB as integer values, but
			// are effectively booleans like the gamefixes
			bool mode = game.speedHacks.at(key) ? 1 : 0;
			dest.Speedhacks.Set(id, mode);
			log_cb(RETRO_LOG_INFO, "(GameDB) Setting Speedhack '%s to [mode=%d]\n", key.c_str(), mode);
			gf++;
		}
	}

	// TODO - config - this could be simplified with maps instead of bitfields and enums
	for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; id++)
	{
		const wxChar *str = tbl_GamefixNames[id];
		std::string key   = wxString(str).ToStdString() + "Hack";

		// Gamefixes are already guaranteed to be valid, any invalid ones are dropped
		if (std::find(game.gameFixes.begin(), game.gameFixes.end(), key) != game.gameFixes.end())
		{
			// if the fix is present, it is said to be enabled
			dest.Gamefixes.Set(id, true);
			log_cb(RETRO_LOG_INFO, "(GameDB) Enabled Gamefix: %s\n", key.c_str());
			gf++;

			// The LUT is only used for 1 game so we allocate it only when the gamefix is enabled (save 4MB)
			if (id == Fix_GoemonTlbMiss && true)
				vtlb_Alloc_Ppmap();
		}
	}

	return gf;
}

// Used to track the current game serial/id, and used to disable verbose logging of
// applied patches if the game info hasn't changed.  (avoids spam when suspending/resuming
// or using TAB or other things), but gets verbose again when booting (even if the same game).
// File scope since it gets reset externally when rebooting
#define _UNKNOWN_GAME_KEY (L"_UNKNOWN_GAME_KEY")
static wxString curGameKey = _UNKNOWN_GAME_KEY;

// fixup = src + command line overrides + game overrides (according to elfCRC).
// While at it, also [re]loads the relevant patches (but doesn't apply them),
// updates the console title, and, for good measures, does some (static) sio stuff.
// Oh, and updates curGameKey. I think that's it.
// It doesn't require that the emulation is paused, and console writes/title should
// be thread safe, but it's best if things don't move around much while it runs.
static void _ApplySettings(const Pcsx2Config& src, Pcsx2Config& fixup)
{
	static Threading::Mutex mtx__ApplySettings;
	Threading::ScopedLock lock(mtx__ApplySettings);
	// 'fixup' is the EmuConfig we're going to upload to the emulator, which very well may
	// differ from the user-configured EmuConfig settings.  So we make a copy here and then
	// we apply the commandline overrides and database gamefixes, and then upload 'fixup'
	// to the global EmuConfig.
	//
	// Note: It's important that we apply the commandline overrides *before* database fixes.
	// The database takes precedence (if enabled).

	fixup = src;

	const CommandlineOverrides& overrides(wxGetApp().Overrides);
	if (overrides.DisableSpeedhacks || !g_Conf->EnableSpeedHacks)
		fixup.Speedhacks.DisableAll();

	if (overrides.ApplyCustomGamefixes)
	{
		for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; ++id)
			fixup.Gamefixes.Set(id, overrides.Gamefixes.Get(id));
	}
	else if (!g_Conf->EnableGameFixes)
		fixup.Gamefixes.DisableAll();

	wxString gameCRC;
	wxString gameMemCardFilter;

	// The CRC can be known before the game actually starts (at the bios), so when
	// we have the CRC but we're still at the bios and the settings are changed
	// (e.g. the user presses TAB to speed up emulation), we don't want to apply the
	// settings as if the game is already running (title, loadeding patches, etc).
	bool ingame = (ElfCRC && (g_GameLoading || g_GameStarted));
	if (ingame)
	{
		gameCRC.Printf(L"%8.8x", ElfCRC);
		log_cb(RETRO_LOG_INFO, "Game CRC: %8.8x\n", ElfCRC);
	}
	else
		gameCRC = L""; // Needs to be reset when rebooting otherwise previously loaded patches may load
		
	const wxString newGameKey(ingame ? SysGetDiscID() : SysGetBiosDiscID());

	curGameKey = newGameKey;

	ForgetLoadedPatches();

	if (!curGameKey.IsEmpty())
	{
		if (IGameDatabase* GameDB = wxGetApp().GetGameDatabase())
		{
			GameDatabaseSchema::GameEntry game = GameDB->findGame(std::string(curGameKey));
			if (game.isValid)
				gameMemCardFilter = game.memcardFiltersAsString();

			if (fixup.EnablePatches)
			{
				if (int patches = LoadPatchesFromGamesDB(gameCRC, game))
				{
					log_cb(RETRO_LOG_INFO, "(GameDB) Patches Loaded: %d\n", patches);
				}
				if (int fixes = loadGameSettings(fixup, game))
					log_cb(RETRO_LOG_INFO, "(GameDB) Fixes Loaded: %d\n", fixes);
			}
		}
	}

	if (!gameMemCardFilter.IsEmpty())
		sioSetGameSerial(gameMemCardFilter);
	else
		sioSetGameSerial(curGameKey);

	//Till the end of this function, entry CRC will be 00000000
	if (!gameCRC.Length())
	{
		log_cb(RETRO_LOG_WARN, "Patches: No CRC found, using 00000000 instead.\n");
		gameCRC = L"00000000";
	}
	else
	{
		// regular cheat patches
		if (fixup.EnableCheats)
		{
			log_cb(RETRO_LOG_INFO, "Attempt to apply cheats if available...\n");
			int numcheatsfound = 0;
			if ((numcheatsfound = LoadPatchesFromDir(gameCRC, GetCheatsFolder(), L"Cheats")) > 0) {
				if (!msg_cheats_found_sent)
				{
					RetroMessager::Notification("Found and applied cheats");
					log_cb(RETRO_LOG_INFO, "Found and applied cheats\n");
					msg_cheats_found_sent = true;
				}
			}
			log_cb(RETRO_LOG_INFO, "(GameDB) Cheats Loaded: %d\n", numcheatsfound);
		}


		// wide screen patches
		if (fixup.EnableWideScreenPatches)
		{
			log_cb(RETRO_LOG_INFO, "Attempt to apply widescreen patches if available...\n");
			if (LoadPatchesFromDir(gameCRC, GetCheatsWsFolder(), L"Widescreen hacks") > 0)
			{
				log_cb(RETRO_LOG_INFO, "Found widescreen patches in the cheats_ws folder --> skipping cheats_ws.zip\n");
				if (!msg_cheat_ws_found_sent) 
				{
					RetroMessager::Notification("Found and applied Widescreen Patch");
					log_cb(RETRO_LOG_INFO, "Found and applied Widescreen Patch\n");
					msg_cheat_ws_found_sent = true;
				}
			}
			else
			{
				// No ws cheat files found at the cheats_ws folder, try the ws cheats zip file.
				int numberDbfCheatsLoaded = LoadWidescreenPatchesFromDatabase(gameCRC.ToStdString());
				log_cb(RETRO_LOG_INFO, "(Wide Screen Cheats DB) Patches Loaded: %d\n", numberDbfCheatsLoaded);

				if (numberDbfCheatsLoaded)
				{
					if (!msg_cheat_ws_found_sent)
					{
						RetroMessager::Notification("Found and applied Widescreen Patch");
						log_cb(RETRO_LOG_INFO, "Found and applied Widescreen Patch\n");
						msg_cheat_ws_found_sent = true;
					}
				}

			}

			/* If we applied a widescreen hack, we need to update the aspect ratio on the libretro side */
			if (msg_cheat_ws_found_sent) 
			{
				struct retro_system_av_info info;
				retro_get_system_av_info(&info);
				environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &info.geometry);
			}
		}

		// 60fps patches
		if (fixup.Enable60fpsPatches)
		{
			log_cb(RETRO_LOG_INFO, "Attempt to apply 60fps patches if available...\n");
			int numberDbfCheatsLoaded = Load60fpsPatchesFromDatabase(gameCRC.ToStdString());
			log_cb(RETRO_LOG_INFO, "(60fps Cheats DB) Patches Loaded: %d\n", numberDbfCheatsLoaded);

			if (numberDbfCheatsLoaded) {
				if (!msg_cheat_60fps_found_sent)
				{
					RetroMessager::Notification("Found and applied 60fps Patch");
					log_cb(RETRO_LOG_INFO, "Found and applied 60fps Patch\n");
					msg_cheat_60fps_found_sent = true;
				}
			}
		}

		if (fixup.EnableNointerlacingPatches)
		{
			// Try applying cheats nointerlacing zip file.
			int numberDbfCheatsLoaded = LoadNointerlacingPatchesFromDatabase(gameCRC.ToStdString());
			log_cb(RETRO_LOG_INFO, "(No-interlacing Cheats DB) Patches Loaded: %d\n", numberDbfCheatsLoaded);

			if (numberDbfCheatsLoaded) {
				if (!msg_cheat_nointerlacing_found_sent)
				{
					RetroMessager::Notification("Found and applied No-interlacing Patch");
					log_cb(RETRO_LOG_INFO, "Found and applied No-interlacing Patch\n");
					msg_cheat_nointerlacing_found_sent = true;
				}
			}
		}
	}
}

// FIXME: This function is not for general consumption. Its only consumer (and
//        the only place it's declared at) is i5900-32.cpp .
// It's here specifically to allow loading the patches synchronously (to the caller)
// when the recompiler detects the game's elf entry point, such that the patches
// are applied to the elf in memory before it's getting recompiled.
// TODO: Find a way to pause the recompiler once it detects the elf entry, then
//       make AppCoreThread::ApplySettings run more normally, and then resume
//       the recompiler.
//       The key point is that the patches should be loaded exactly before the elf
//       is recompiled. Loading them earlier might incorrectly patch the bios memory,
//       and later might be too late since the code was already recompiled
void LoadAllPatchesAndStuff(const Pcsx2Config& cfg)
{
	Pcsx2Config dummy;
	curGameKey = _UNKNOWN_GAME_KEY;
	_ApplySettings(cfg, dummy);
}

void AppCoreThread::ApplySettings(const Pcsx2Config& src)
{
	// Re-entry guard protects against cases where code wants to manually set core settings
	// which are not part of g_Conf.  The subsequent call to apply g_Conf settings (which is
	// usually the desired behavior) will be ignored.
	Pcsx2Config fixup;
	_ApplySettings(src, fixup);

	static int localc = 0;
	RecursionGuard guard(localc);
	if (guard.Counter > 1)
		return;
	if (fixup == EmuConfig)
		return;

	_parent::ApplySettings(fixup);
}

// --------------------------------------------------------------------------------------
//  AppCoreThread *Worker* Implementations
//    (Called from the context of this thread only)
// --------------------------------------------------------------------------------------

void AppCoreThread::DoCpuReset()
{
	PostCoreStatus(CoreThread_Reset);
	_parent::DoCpuReset();
}

void AppCoreThread::OnResumeInThread(bool isSuspended)
{
	if (m_resetCdvd)
	{
		CDVDsys_ChangeSource(g_Conf->CdvdSource);
		cdvdCtrlTrayOpen();
		m_resetCdvd = false;
	}

	_parent::OnResumeInThread(isSuspended);
	PostCoreStatus(CoreThread_Resumed);
}

void AppCoreThread::OnSuspendInThread()
{
	_parent::OnSuspendInThread();
	PostCoreStatus(CoreThread_Suspended);
}

// Called whenever the thread has terminated, for either regular or irregular reasons.
// Typically the thread handles all its own errors, so there's no need to have error
// handling here.  However it's a good idea to update the status of the GUI to reflect
// the new (lack of) thread status, so this posts a message to the App to do so.
void AppCoreThread::OnCleanupInThread()
{
	m_ExecMode = ExecMode_Closing;
	PostCoreStatus(CoreThread_Stopped);
	_parent::OnCleanupInThread();
}

void AppCoreThread::GameStartingInThread()
{
	// Simulate a Close/Resume, so that settings get re-applied and the database
	// lookups and other game-based detections are done.

	m_ExecMode = ExecMode_Paused;
	OnResumeReady();
	_reset_stuff_as_needed();
	ClearMcdEjectTimeoutNow(); // probably safe to do this when a game boots, eliminates annoying prompts
	m_ExecMode = ExecMode_Opened;

	_parent::GameStartingInThread();
}

bool AppCoreThread::StateCheckInThread()
{
	return _parent::StateCheckInThread();
}

static uint m_except_threshold = 0;

void AppCoreThread::ExecuteTaskInThread()
{
	PostCoreStatus(CoreThread_Started);
	m_except_threshold = 0;
	_parent::ExecuteTaskInThread();
}

void AppCoreThread::DoCpuExecute()
{
	try
	{
		_parent::DoCpuExecute();
	}
	catch (BaseR5900Exception& ex)
	{
		// [TODO] : Debugger Hook!

		if (++m_except_threshold > 6)
		{
			// If too many TLB Misses occur, we're probably going to crash and
			// the game is probably running miserably.

			m_except_threshold = 0;

			log_cb(RETRO_LOG_ERROR, "Too many execution errors.  VM execution has been suspended!\n");

			// Hack: this keeps the EE thread from 
			// running more code while the SysExecutor
			// thread catches up and signals it for suspension.
			m_ExecMode = ExecMode_Closing;
		}
	}
}
