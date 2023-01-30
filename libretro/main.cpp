
#include "Utilities/Dependencies.h"

#ifdef WIN32
#include <windows.h>
#undef Yield
#endif

#include "App.h"

#include <cstdint>
#include <libretro.h>
#include <libretro_core_options.h>
#include <string>
#include <thread>
#include <vector>
#include <wx/textfile.h>
#include <wx/dir.h>
#include <wx/evtloop.h>

#include "../pcsx2/GS.h"
#include "options_tools.h"
#include "retro_messager.h"
#include "language_injector.h"
#include "input.h"
#include "disk_control.h"
#include "../pcsx2/ps2/BiosTools.h"
#include "memcard_retro.h"
#include "../pcsx2/PAD/PAD.h"

#include "../pcsx2/MTVU.h"
#include "../pcsx2/GS/GSFuncs.h"

#ifdef PERF_TEST
#define RETRO_PERFORMANCE_INIT(name)                 \
	retro_perf_tick_t current_ticks;                 \
	static struct retro_perf_counter name = {#name}; \
	if (!name.registered)                            \
		perf_cb.perf_register(&(name));              \
	current_ticks = name.total

#define RETRO_PERFORMANCE_START(name) perf_cb.perf_start(&(name))
#define RETRO_PERFORMANCE_STOP(name) \
	perf_cb.perf_stop(&(name));      \
	current_ticks = name.total - current_ticks;
#else
#define RETRO_PERFORMANCE_INIT(name)
#define RETRO_PERFORMANCE_START(name)
#define RETRO_PERFORMANCE_STOP(name)
#endif

#define FILENAME_SHARED_MEMCARD_8 "Shared Memory Card (8 MB)"
#define FILENAME_SHARED_MEMCARD_32 "Shared Memory Card (32 MB)"

retro_audio_sample_t sample_cb;
retro_environment_t environ_cb;
retro_video_refresh_t video_cb;
retro_log_printf_t log_cb;

struct retro_hw_render_callback hw_render;

#ifdef PERF_TEST
static struct retro_perf_callback perf_cb;
#endif

struct retro_rumble_interface rumble;
static retro_input_poll_t poll_cb;

static struct retro_input_descriptor desc[] = {
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Triangle"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Circle"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Cross"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Square"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L1"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R1"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
	{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "L-Analog X"},
	{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "L-Analog Y"},
	{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "R-Analog X"},
	{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "R-Analog Y"},

	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Triangle"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Circle"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Cross"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Square"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L1"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R1"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
	{1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "L-Analog X"},
	{1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "L-Analog Y"},
	{1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "R-Analog X"},
	{1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "R-Analog Y"},

	{0},
};

static wxFileName bios_dir;

static wxFileName slot1_file;
static wxFileName slot2_file;

static wxFileName legacy_memcard1;
static wxFileName legacy_memcard2;

static std::vector<std::string> bios_files;
static std::vector<std::string> custom_memcard_list_slot1;
static std::vector<std::string> custom_memcard_list_slot2;

static bool libretro_supports_option_categories = false;
static bool init_failed                         = false;
int option_upscale_mult                         = 1;
int option_pad_left_deadzone                    = 0;
int option_pad_right_deadzone                   = 0;
bool option_palette_conversion                  = false;
bool hack_fb_conversion                         = false;
bool hack_AutoFlush                             = false;
bool hack_fast_invalidation                     = false;
bool hack_preload_frame_data                    = false;

std::string sel_bios_path                       = "";
unsigned libretro_msg_interface_version         = 0;

std::string retroarch_system_path;

Pcsx2App* pcsx2;

/* forward declaration */
extern Pad pads[2][4];

void retro_set_controller_port_device(unsigned port, unsigned device)
{
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

void retro_set_input_poll(retro_input_poll_t cb)
{
	poll_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;
	bool no_game = true;
	environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
#ifdef PERF_TEST
	environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb);
#endif
}

void retro_init(void)
{
	wxFileName save_dir_root;
	wxFileName f_bios;
	wxArrayString bios_list;
	wxArrayString memcard_files_slot1;
	wxArrayString memcard_files_slot2;
	struct retro_log_callback log;
	const char* system   = NULL;
	const char* save_dir = NULL;
	enum retro_pixel_format xrgb888 = RETRO_PIXEL_FORMAT_XRGB8888;

	environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);

	if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
		log_cb = log.log;

	environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &libretro_msg_interface_version);
	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system);
	environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir);

	retroarch_system_path = system;

	// checks and create save folders

	save_dir_root = wxFileName(wxString(save_dir), "");
	slot1_file    = wxFileName(save_dir_root.GetPath(), "");
	slot2_file    = wxFileName(save_dir_root.GetPath(), "");
	save_dir_root.AppendDir("pcsx2");
	slot1_file.AppendDir("Slot 1");
	slot2_file.AppendDir("Slot 2");
	
	if (! save_dir_root.DirExists())
		save_dir_root.Mkdir();
	if (!slot1_file.DirExists())
		slot1_file.Mkdir();
	if (!slot2_file.DirExists())
		slot2_file.Mkdir();

	// check if legacy memcards exists

	wxFileName legacy_dir(wxString(system), "");
	legacy_dir.AppendDir("pcsx2");
	legacy_dir.AppendDir("memcards");

	legacy_memcard1 = wxFileName(legacy_dir.GetPath(), "");
	legacy_memcard1.SetName("Mcd001");
	legacy_memcard1.SetExt("ps2");

	legacy_memcard2 = wxFileName(legacy_dir.GetPath(), "");
	legacy_memcard2.SetName("Mcd002");
	legacy_memcard2.SetExt("ps2");

	// get other 'custom' memcards put by the user in the slot 1 folder, if any
	wxDir::GetAllFiles(slot1_file.GetPath(), &memcard_files_slot1, L"*.*", wxDIR_FILES);
	for (wxString memcard_file : memcard_files_slot1)
	{
		wxFileName found_file;
		found_file.Assign(memcard_file);
		if (!found_file.GetName().IsSameAs(FILENAME_SHARED_MEMCARD_8) && !found_file.GetName().IsSameAs(FILENAME_SHARED_MEMCARD_32))
		{
			if (found_file.GetExt().IsSameAs("ps2"))
			{
				custom_memcard_list_slot1.push_back((std::string)found_file.GetFullPath());

				std::string name = (std::string)found_file.GetName();
				name.insert(0, "[USER] ");
				custom_memcard_list_slot1.push_back(name);
			}
		}
		
	}

	// get other 'custom' memcards put by the user in the slot 2 folder, if any
	wxDir::GetAllFiles(slot2_file.GetPath(), &memcard_files_slot2, L"*.*", wxDIR_FILES);
	for (wxString memcard_file : memcard_files_slot2)
	{
		wxFileName found_file;
		found_file.Assign(memcard_file);
		if (!found_file.GetName().IsSameAs(FILENAME_SHARED_MEMCARD_8) && !found_file.GetName().IsSameAs(FILENAME_SHARED_MEMCARD_32))
		{
			if (found_file.GetExt().IsSameAs("ps2"))
			{
				custom_memcard_list_slot2.push_back((std::string)found_file.GetFullPath());

				std::string name = (std::string)found_file.GetName();
				name.insert(0, "[USER] ");
				custom_memcard_list_slot2.push_back(name);
			}
		}
	}


	// breaking after filled should be more quick than looping throught all options
	// so we do this 2 times for both memcard options slot.
	// the max number of shared memcard is limited to 20
	//
	// Per game folders saves has been disabled because hangs while writing on disk
	// and seems not working well on some games
	for (retro_core_option_v2_definition& def : option_defs_us)
	{								
		size_t i = 0, j;
		if (!def.key || strcmp(def.key, "pcsx2_memcard_slot_1")) continue;
		def.values[i++]         = { "empty",   "Empty"                    };
		if (legacy_memcard1.FileExists())
			def.values[i++] = {"legacy",   "Legacy"                   };
		def.values[i++]         = {"shared8",  FILENAME_SHARED_MEMCARD_8  };
		def.values[i++]         = {"shared32", FILENAME_SHARED_MEMCARD_32 };
		for (j = 0; j < custom_memcard_list_slot1.size(); j += 2)
		{
			if (j >= 40) break;
			def.values[i++] = { custom_memcard_list_slot1[j].c_str(), custom_memcard_list_slot1[j + 1].c_str()};
		}

		def.values[i++]         = { NULL, NULL };
		if (i > 1)
			def.default_value = def.values[1].value;
		break;
	}

	for (retro_core_option_v2_definition& def : option_defs_us)
	{
		size_t i = 0, j;
		if (!def.key || strcmp(def.key, "pcsx2_memcard_slot_2")) continue; 
		def.values[i++]         = {"empty",    "Empty"                    };
		if (legacy_memcard2.FileExists())
			def.values[i++] = {"legacy",   "Legacy"                   };
		def.values[i++]         = {"shared8",  FILENAME_SHARED_MEMCARD_8  };
		def.values[i++]         = {"shared32", FILENAME_SHARED_MEMCARD_32 };
		for (j = 0; j < custom_memcard_list_slot2.size(); j += 2)
		{
			if (j >= 40) break;
			def.values[i++] = { custom_memcard_list_slot2[j].c_str(), custom_memcard_list_slot2[j + 1].c_str()};
		}

		def.values[i++]         = { NULL, NULL };
		def.default_value       = def.values[0].value;
		break;
	}


	// get the BIOS available and fill the option

	bios_dir = Path::Combine(system, "pcsx2/bios");
	wxDir::GetAllFiles(bios_dir.GetFullPath(), &bios_list, L"*.*", wxDIR_FILES);
	for (wxString bios_file : bios_list)
	{
			wxString description;
			if (IsBIOS(bios_file, description))
			{
				std::string log_bios = (std::string)description;
				wxFileName f;
				f.Assign(bios_file);
				bios_files.push_back((std::string)f.GetFullName());
				bios_files.push_back((std::string)description);
			}
	}

	// check if there are bios file in the bios folders, otherwise terminates with a explicit log message

	if (bios_files.size() == 0)
	{
		std::string checked_path = bios_dir.GetFullPath().ToStdString();
		log_cb(RETRO_LOG_ERROR, "Could not find valid BIOS files! \n");
		log_cb(RETRO_LOG_ERROR, "Please provide required BIOS file in the following folder: '%s' \n", checked_path.c_str());
		return;
	}


	for (retro_core_option_v2_definition& def : option_defs_us)
	{
		int cont = 0;
		size_t f, numfiles, i = 0;
		if (!def.key || strcmp(def.key, "pcsx2_bios")) continue;
		numfiles = bios_files.size();
		for (f = 0; f != numfiles; f += 2)
		{
			def.values[i++] = { bios_files[f].c_str(), bios_files[f + 1].c_str() };
			cont++;
		}
		def.values[cont] = { NULL, NULL };
		def.default_value = def.values[0].value;
		break;
	}


	// loads the options structure to the frontend

	libretro_supports_option_categories = false;
	libretro_set_core_options(environ_cb,
		&libretro_supports_option_categories);

	// start init some core settings
	option_upscale_mult       = option_value(INT_PCSX2_OPT_UPSCALE_MULTIPLIER, KeyOptionInt::return_type);
	option_palette_conversion = option_value(BOOL_PCSX2_OPT_PALETTE_CONVERSION, KeyOptionBool::return_type);
	hack_fb_conversion        = option_value(BOOL_PCSX2_OPT_USERHACK_FB_CONVERSION, KeyOptionBool::return_type);
	hack_AutoFlush            = option_value(BOOL_PCSX2_OPT_USERHACK_AUTO_FLUSH, KeyOptionBool::return_type);
	hack_fast_invalidation    = option_value(BOOL_PCSX2_OPT_USERHACK_FAST_INVALIDATION, KeyOptionBool::return_type);
	hack_preload_frame_data   = option_value(BOOL_PCSX2_OPT_USERHACK_PRELOAD_FRAME_DATA, KeyOptionBool::return_type);

	f_bios.Assign(option_value(STRING_PCSX2_OPT_BIOS, KeyOptionString::return_type));

	f_bios                    = wxFileName(bios_dir.GetFullPath(), option_value(STRING_PCSX2_OPT_BIOS, KeyOptionString::return_type));
	sel_bios_path             = f_bios.GetFullPath().ToStdString();
	
	// instantiate the pcsx2 app and so some things on it

	pcsx2                    = &wxGetApp();
	pxDoOutOfMemory          = SysOutOfMemory_EmergencyResponse;
	
	g_Conf                   = std::make_unique<AppConfig>();
	g_Conf->EmuOptions.BiosFilename.Assign(sel_bios_path);
	
	// some other stuffs about pcsx2

	if (pcsx2->DetectCpuAndUserMode())
	{
		pcsx2->AllocateCoreStuffs();

		// checks and creates the shared memory cards based on user option if
		// already set (e.g. global config + new content)

		if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type), "shared8") == 0)
		{
			slot1_file.SetName(FILENAME_SHARED_MEMCARD_8);
			slot1_file.SetExt("ps2");
			MemCardRetro::CreateSharedMemCardIfNotExisting(slot1_file, 8);

		}
		else if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type), "shared32") == 0)
		{
			slot1_file.SetName(FILENAME_SHARED_MEMCARD_32);
			slot1_file.SetExt("ps2");
			MemCardRetro::CreateSharedMemCardIfNotExisting(slot1_file, 32);
		}


		if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type), "shared8") == 0)
		{
			slot2_file.SetName(FILENAME_SHARED_MEMCARD_8);
			slot2_file.SetExt("ps2");
			MemCardRetro::CreateSharedMemCardIfNotExisting(slot2_file, 8);

		}
		else if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type), "shared32") == 0)
		{
			slot2_file.SetName(FILENAME_SHARED_MEMCARD_32);
			slot2_file.SetExt("ps2");
			MemCardRetro::CreateSharedMemCardIfNotExisting(slot2_file, 32);
		}

		// apply options to LRPS2

		g_Conf->EnablePresets                              = true;
		g_Conf->EmuOptions.Speedhacks.fastCDVD             = option_value(BOOL_PCSX2_OPT_FASTCDVD, KeyOptionBool::return_type);

		g_Conf->EmuOptions.EnableNointerlacingPatches      = (option_value(INT_PCSX2_OPT_DEINTERLACING_MODE, KeyOptionInt::return_type) == -1);
		g_Conf->EmuOptions.Enable60fpsPatches              = (option_value(BOOL_PCSX2_OPT_ENABLE_60FPS_PATCHES, KeyOptionBool::return_type));
		g_Conf->EmuOptions.EnableWideScreenPatches         = option_value(BOOL_PCSX2_OPT_ENABLE_WIDESCREEN_PATCHES, KeyOptionBool::return_type);
		g_Conf->EmuOptions.GS.VsyncQueueSize               = option_value(INT_PCSX2_OPT_VSYNC_MTGS_QUEUE, KeyOptionInt::return_type);
		g_Conf->EmuOptions.EnableCheats                    = option_value(BOOL_PCSX2_OPT_ENABLE_CHEATS, KeyOptionBool::return_type);


		int EE_clampMode                                   = option_value(INT_PCSX2_OPT_EE_CLAMPING_MODE, KeyOptionInt::return_type);
		g_Conf->EmuOptions.Cpu.Recompiler.fpuOverflow      = (EE_clampMode >= 1);
		g_Conf->EmuOptions.Cpu.Recompiler.fpuExtraOverflow = (EE_clampMode >= 2);
		g_Conf->EmuOptions.Cpu.Recompiler.fpuFullMode      = (EE_clampMode >= 3);

		SSE_RoundMode EE_roundMode                         = (SSE_RoundMode)option_value(INT_PCSX2_OPT_EE_ROUND_MODE, KeyOptionInt::return_type);
		g_Conf->EmuOptions.Cpu.sseMXCSR.SetRoundMode(EE_roundMode);

		int VUs_clampMode                                  = option_value(INT_PCSX2_OPT_VU_CLAMPING_MODE, KeyOptionInt::return_type);
		g_Conf->EmuOptions.Cpu.Recompiler.vuOverflow       = (VUs_clampMode >= 1);
		g_Conf->EmuOptions.Cpu.Recompiler.vuExtraOverflow  = (VUs_clampMode >= 2);
		g_Conf->EmuOptions.Cpu.Recompiler.vuSignOverflow   = (VUs_clampMode >= 3);

		SSE_RoundMode VUs_roundMode                        = (SSE_RoundMode)option_value(INT_PCSX2_OPT_VU_ROUND_MODE, KeyOptionInt::return_type);
		g_Conf->EmuOptions.Cpu.sseVUMXCSR.SetRoundMode(VUs_roundMode);

		option_pad_left_deadzone                           = option_value(INT_PCSX2_OPT_GAMEPAD_L_DEADZONE, KeyOptionInt::return_type);
		option_pad_right_deadzone                          = option_value(INT_PCSX2_OPT_GAMEPAD_R_DEADZONE, KeyOptionInt::return_type);

		static retro_disk_control_ext_callback disk_control = {
			DiskControl::set_eject_state,
			DiskControl::get_eject_state,
			DiskControl::get_image_index,
			DiskControl::set_image_index,
			DiskControl::get_num_images,
			DiskControl::replace_image_index,
			DiskControl::add_image_index,
			DiskControl::set_initial_image,
			DiskControl::get_image_path,
			DiskControl::get_image_label,
		};

		environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &disk_control);

	}
	else
		init_failed = true;
}

void retro_deinit(void)
{
	libretro_supports_option_categories = false;

	/* FIXME: This is a workaround that resolves crashes on close content.
	When closing the frontend, we end up with a zombie process because the
	main thread tries to call vu1Thread.Cancel() within pcsx2's destructor
	and it gets stuck waiting for a mutex that will never unlock */
	vu1Thread.WaitVU();
	//vu1Thread.Cancel();
	
	if (pcsx2)
	{
		pcsx2->CleanupOnExit();
		pcsx2->OnExit();
	}
	
	bios_files.clear();
	custom_memcard_list_slot1.clear();
	custom_memcard_list_slot2.clear();

#ifdef PERF_TEST
	perf_cb.perf_log();
#endif
}

void retro_get_system_info(retro_system_info* info)
{
	static const int PCSX2_VersionHi    = 1;
	static const int PCSX2_VersionMid   = 7;
	static const int PCSX2_VersionLo    = 0;
#ifdef GIT_REV
	info->library_version               = GIT_REV;
#else
	static char version[]               = "#.#.#";
	version[0]                          = '0' + PCSX2_VersionHi;
	version[2]                          = '0' + PCSX2_VersionMid;
	version[4]                          = '0' + PCSX2_VersionLo;
	info->library_version               = version;
#endif
	info->library_name                  = "LRPS2 (alpha)";
	info->valid_extensions              = "elf|iso|ciso|chd|cso|cue|bin|m3u";
	info->need_fullpath                 = true;
	info->block_extract                 = true;
}

void retro_get_system_av_info(retro_system_av_info* info)
{
	extern bool msg_cheat_ws_found_sent;
	info->geometry.base_width           = 640;
	info->geometry.base_height          = 448;
	if (!(!std::strcmp(option_value(STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type), "Software") || !std::strcmp(option_value(STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type), "Null")))
	{
		int upscale_multiplier       = option_value(INT_PCSX2_OPT_UPSCALE_MULTIPLIER, KeyOptionInt::return_type);
		info->geometry.base_width   *= upscale_multiplier;
		info->geometry.base_height  *= upscale_multiplier;
	}

	info->geometry.max_width             = info->geometry.base_width;
	info->geometry.max_height            = info->geometry.base_height;

	if ((option_value(INT_PCSX2_OPT_ASPECT_RATIO, KeyOptionInt::return_type) != 0) || msg_cheat_ws_found_sent)
		info->geometry.aspect_ratio  = 16.0f / 9.0f;
	else
		info->geometry.aspect_ratio  = 4.0f / 3.0f;
	info->timing.fps                     = (retro_get_region() == RETRO_REGION_NTSC) ? (60.0f / 1.001f) : 50.0f;
	info->timing.sample_rate             = 48000;
}

/* TODO/FIXME - fix this */
void retro_reset(void)
{
	GetMTGS().FinishTaskInThread();
	GetCoreThread().ResetQuick();
	DiskControl::eject_state = false;
}

static void context_reset(void)
{
	GetMTGS().OpenGS();
}

static void context_destroy(void)
{
	GetMTGS().FinishTaskInThread();

	while (pcsx2->HasPendingEvents())
		pcsx2->ProcessPendingEvents();
	GetMTGS().CloseGS();
	while (pcsx2->HasPendingEvents())
		pcsx2->ProcessPendingEvents();
}

static bool set_hw_render(retro_hw_context_type type)
{
	hw_render.context_type       = type;
	hw_render.context_reset      = context_reset;
	hw_render.context_destroy    = context_destroy;
	hw_render.bottom_left_origin = true;
	hw_render.depth              = true;
	hw_render.cache_context      = false;

	switch (type)
	{
		case RETRO_HW_CONTEXT_DIRECT3D:
			hw_render.version_major = 11;
			hw_render.version_minor = 0;
			break;

		case RETRO_HW_CONTEXT_OPENGL_CORE:
			hw_render.version_major = 3;
			hw_render.version_minor = 3;
			break;

		case RETRO_HW_CONTEXT_OPENGL:
			if (set_hw_render(RETRO_HW_CONTEXT_OPENGL_CORE))
				return true;

			hw_render.version_major = 3;
			hw_render.version_minor = 0;
			break;

		case RETRO_HW_CONTEXT_OPENGLES3:
			if (set_hw_render(RETRO_HW_CONTEXT_OPENGL))
				return true;

			hw_render.version_major = 3;
			hw_render.version_minor = 0;
			break;

		case RETRO_HW_CONTEXT_NONE:
			hw_render.version_major = 3;
			hw_render.version_minor = 0;
			break;

		default:
			return false;
	}

	return environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render);
}

static wxVector<wxString> read_m3u_file(const wxFileName& m3u_file)
{
	wxString base_path, line;
	wxVector<wxString> result;
	wxTextFile m3u_data;
	/* This is the UTF-8 representation of U+FEFF. */
	const wxString utf8_bom = "\xEF\xBB\xBF";

	log_cb(RETRO_LOG_DEBUG, "Reading M3U file...\n");

	if (!m3u_data.Open(m3u_file.GetFullPath()))
	{
		log_cb(RETRO_LOG_ERROR, "M3U file \"%s\" cannot be read", m3u_file.GetFullPath().c_str());
		return result;
	}

	base_path = m3u_file.GetPath();
	line      = m3u_data.GetFirstLine();

	if (line.StartsWith(utf8_bom))
	{
		log_cb(RETRO_LOG_WARN, "M3U file \"%s\" contains UTF-8 BOM", m3u_file.GetFullPath().c_str());
		line.erase(0, utf8_bom.length());
	}

	for (line; !m3u_data.Eof(); line = m3u_data.GetNextLine())
	{
		if (!line.StartsWith('#')) // Comments start with #
		{
			wxFileName discFile(base_path, line);
			discFile.Normalize();
			if (discFile.Exists())
			{
				log_cb(RETRO_LOG_DEBUG, "Found disc image in M3U file, %s\n", discFile.GetFullPath());
				result.push_back(discFile.GetFullPath());
			}
			else
			{
				wxString full_path = discFile.GetFullPath();
				log_cb(RETRO_LOG_WARN, "File specified in the M3U file \"%s\" was not found:\n%s\n", m3u_file.GetFullPath().c_str(), full_path.c_str());
			}
		}
	}

	if (result.empty())
		log_cb(RETRO_LOG_ERROR, "No paths found in the M3U file \"%s\"\n", m3u_file.GetFullPath().c_str());
	return result;
}

bool retro_load_game(const struct retro_game_info* game)
{
	static const struct retro_controller_description ds2_desc[] = {
		{"DualShock 2", RETRO_DEVICE_JOYPAD},
	};

	static const struct retro_controller_info ports[] = {
		{ds2_desc, sizeof(ds2_desc) / sizeof(*ds2_desc)},
		{ds2_desc, sizeof(ds2_desc) / sizeof(*ds2_desc)},
		{},
	};

	bool fastboot_option;
	wxFileName nvmFileCheck;
	const char *selected_bios = NULL;
	const char* system        = NULL;

	if (init_failed)
	{
		init_failed = false;
		return false;
	}

	ResetContentStuffs();

	if (sel_bios_path.empty())
	{
		log_cb(RETRO_LOG_ERROR, "Could not find any valid PS2 BIOS File in %s\n", (const char*)bios_dir.GetFullPath());
		return false;
	}
		
	selected_bios = sel_bios_path.c_str();
	
	log_cb(RETRO_LOG_INFO, "Loading selected BIOS:  %s\n", selected_bios);

	// we check if NVM file exists, if not it means that it's a new BIOS.

	nvmFileCheck.Assign(sel_bios_path);
	nvmFileCheck.SetExt("nvm");
	
	fastboot_option            = option_value(BOOL_PCSX2_OPT_FASTBOOT, KeyOptionBool::return_type);

	// in this case we bypass the fastboot option, forcing to enter the BIOS first run setup
	if (! nvmFileCheck.FileExists())
		fastboot_option    = false;

	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system);

	// By default no IRX injection
	g_Conf->CurrentIRX         = "";
	g_Conf->BaseFilenames.Bios = selected_bios;

	DiskControl::eject_state   = false;


	// set up memcard on slot 1

	if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type), "empty") == 0)
	{
		// slot empty
		g_Conf->Mcd[0].Type     = MemoryCardType::MemoryCard_None;
		g_Conf->Mcd[0].Enabled  = false;
	}
	else if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type), "shared8") == 0
		|| strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type), "shared32") == 0)
	{
		// Shared Memcards
		g_Conf->Mcd[0].Type     = MemoryCardType::MemoryCard_File;
		g_Conf->Mcd[0].Enabled  = true;
		g_Conf->Mcd[0].Filename = slot1_file;

	}
	else if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type), "legacy") == 0)
	{
		// legacy
		g_Conf->Mcd[0].Type     = MemoryCardType::MemoryCard_File;
		g_Conf->Mcd[0].Enabled  = true;
		g_Conf->Mcd[0].Filename = legacy_memcard1;
	}
	else
	{
		// User imported memcards
		wxFileName user_memcard;
		user_memcard.Assign(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type));

		g_Conf->Mcd[0].Type     = MemoryCardType::MemoryCard_File;
		g_Conf->Mcd[0].Enabled  = true;
		g_Conf->Mcd[0].Filename = user_memcard;
	}


	// set up memcard on slot 2

	if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type), "empty") == 0)
	{
		// slot empty
		g_Conf->Mcd[1].Type     = MemoryCardType::MemoryCard_None;
		g_Conf->Mcd[1].Enabled  = false;
	}
	else if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type), "shared8") == 0
		|| strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type), "shared32") == 0)
	{
		// Shared Memcards
		g_Conf->Mcd[1].Type     = MemoryCardType::MemoryCard_File;
		g_Conf->Mcd[1].Enabled  = true;
		g_Conf->Mcd[1].Filename = slot2_file;
	}
	else if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type), "legacy") == 0)
	{
		// legacy
		g_Conf->Mcd[1].Type     = MemoryCardType::MemoryCard_File;
		g_Conf->Mcd[1].Enabled  = true;
		g_Conf->Mcd[1].Filename = legacy_memcard2;
	}
	else
	{
		// User imported memcards
		wxFileName user_memcard;
		user_memcard.Assign(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type));

		g_Conf->Mcd[1].Type = MemoryCardType::MemoryCard_File;
		g_Conf->Mcd[1].Enabled = true;
		g_Conf->Mcd[1].Filename = user_memcard;

	}

	if (game)
	{
		u32 magic = 0;
		FILE *fp  = NULL;
		wxVector<wxString> game_paths;

		LanguageInjector::Inject(
				sel_bios_path,
				option_value(STRING_PCSX2_OPT_SYSTEM_LANGUAGE, KeyOptionString::return_type)
				);

		wxFileName file_name(game->path);
		if (file_name.GetExt() == "m3u")
		{
			game_paths = read_m3u_file(file_name);
			log_cb(RETRO_LOG_DEBUG, "Found %u game images in M3U file.\n", game_paths.size());
		}
		else
			game_paths.push_back(game->path);

		DiskControl::disk_images.assign(game_paths.begin(), game_paths.end());

		if (!(fp = fopen(game_paths[0], "rb")))
		{
			log_cb(RETRO_LOG_ERROR, "Could not open file: %s\n", game_paths[0]);
			return false;
		}

		fread(&magic, 4, 1, fp);
		fclose(fp);

		if (magic == 0x464C457F) /* ELF */
		{
			// g_Conf->CurrentIRX = "";
			log_cb(RETRO_LOG_INFO, "ELF file detected, loading content....\n");
			g_Conf->EmuOptions.UseBOOT2Injection = true;
			pcsx2->SysExecute(CDVD_SourceType::NoDisc, game_paths[0]);
		}
		else
		{	
			g_Conf->EmuOptions.UseBOOT2Injection = fastboot_option;
			g_Conf->CdvdSource                   = CDVD_SourceType::Iso;
			g_Conf->CurrentIso                   = game_paths[0];

			if (!option_value(BOOL_PCSX2_OPT_BOOT_TO_BIOS, KeyOptionBool::return_type))
				pcsx2->SysExecute(g_Conf->CdvdSource);
			else
			{
				// We enter here in the BIOS, so we have the correct memcards loaded
				log_cb(RETRO_LOG_INFO, "Entering BIOS Menu.....\n");
				RetroMessager::Notification("Boot to BIOS enabled", true);
				g_Conf->EmuOptions.UseBOOT2Injection = false;
				g_Conf->CdvdSource = CDVD_SourceType::NoDisc;
				g_Conf->CurrentIso = "";
				pcsx2->SysExecute(g_Conf->CdvdSource);

			}
		}

	}
	else
	{
		log_cb(RETRO_LOG_INFO, "Entering BIOS Menu.....\n");
		g_Conf->EmuOptions.UseBOOT2Injection = false;
		g_Conf->CdvdSource = CDVD_SourceType::NoDisc;
		g_Conf->CurrentIso = "";
		pcsx2->SysExecute(g_Conf->CdvdSource);
	}

	environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble);
	environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
	//	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

	Input_RumbleEnabled(
			option_value(BOOL_PCSX2_OPT_GAMEPAD_RUMBLE_ENABLE, KeyOptionBool::return_type),
			option_value(INT_PCSX2_OPT_GAMEPAD_RUMBLE_FORCE, KeyOptionInt::return_type)
			);

	retro_hw_context_type context_type = RETRO_HW_CONTEXT_OPENGL;
	const char* option_renderer = option_value(STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type);
	log_cb(RETRO_LOG_INFO, "Renderer option set to: %s\n", option_renderer);

	if (!std::strcmp(option_renderer,"Auto"))
	{
		environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &context_type);
		/* Check if the selected video driver is supported, switch to OpenGL otherwise. */
		if (context_type != RETRO_HW_CONTEXT_NONE && set_hw_render(context_type))
			return true;

		context_type = RETRO_HW_CONTEXT_OPENGL;
		log_cb(RETRO_LOG_INFO, "The video driver found is not compatible, switched to OpenGL.\n");
	}
#ifdef _WIN32
	else if (!std::strcmp(option_renderer, "D3D11"))
		context_type = RETRO_HW_CONTEXT_DIRECT3D;
#endif
	else if (!std::strcmp(option_renderer, "Null"))
		context_type = RETRO_HW_CONTEXT_NONE;

	if (!set_hw_render(context_type))
		return false;
	return true;
}

void retro_unload_game(void)
{
	//	GetMTGS().FinishTaskInThread();
	//		GetMTGS().CloseGS();
	GetMTGS().FinishTaskInThread();

	while (pcsx2->HasPendingEvents())
		pcsx2->ProcessPendingEvents();
	GetMTGS().CloseGS();

	while (pcsx2->HasPendingEvents())
		pcsx2->ProcessPendingEvents();

	init_failed = false;
	ResetContentStuffs();
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info,
		size_t num_info)
{
	return false;
}

void retro_run(void)
{
	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
	{
		EmuConfig.GS.VsyncQueueSize = option_value(INT_PCSX2_OPT_VSYNC_MTGS_QUEUE, KeyOptionInt::return_type);
		GSUpdateOptions();
		Input_RumbleEnabled(
			option_value(BOOL_PCSX2_OPT_GAMEPAD_RUMBLE_ENABLE, KeyOptionBool::return_type),
			option_value(INT_PCSX2_OPT_GAMEPAD_RUMBLE_FORCE, KeyOptionInt::return_type)
		);
		option_pad_left_deadzone  = option_value(INT_PCSX2_OPT_GAMEPAD_L_DEADZONE, KeyOptionInt::return_type);
		option_pad_right_deadzone = option_value(INT_PCSX2_OPT_GAMEPAD_R_DEADZONE, KeyOptionInt::return_type);

	}

	poll_cb();

	for (unsigned port = 0; port < 2; port++)
		for (unsigned slot = 0; slot < 4; slot++)
			pads[port][slot].rumble(port);

	RETRO_PERFORMANCE_INIT(pcsx2_run);
	RETRO_PERFORMANCE_START(pcsx2_run);

	GetMTGS().ExecuteTaskInThread();

	RETRO_PERFORMANCE_STOP(pcsx2_run);
}

/* TODO/FIXME - implement */
size_t retro_serialize_size(void)                     { return 0; }
bool retro_serialize(void* data, size_t size)         { return false; }
bool retro_unserialize(const void* data, size_t size) { return false; }

/* TODO/FIXME - properly implement */
unsigned retro_get_region(void)                       { return RETRO_REGION_NTSC; }
unsigned retro_api_version(void)                      { return RETRO_API_VERSION; }
size_t retro_get_memory_size(unsigned id)             { return 0; }
void* retro_get_memory_data(unsigned id)              { return NULL; }

/* TODO/FIXME - implement */
void retro_cheat_reset(void)                                         { }
void retro_cheat_set(unsigned index, bool enabled, const char* code) { }

/* TODO/FIXME - should maybe go for sample batch cb for improved
 * efficiency */
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {}
void retro_set_audio_sample(retro_audio_sample_t cb)             {sample_cb = cb; }

wxEventLoopBase* Pcsx2AppTraits::CreateEventLoop()
{
	return new wxEventLoop();
}
