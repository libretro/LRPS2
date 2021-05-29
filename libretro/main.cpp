
#include "PrecompiledHeader.h"

#ifdef WIN32
#include <windows.h>
#undef Yield
#endif

#include "AppCommon.h"
#include "App.h"

#include <cstdint>
#include <libretro.h>
#include <libretro_core_options.h>
#include <string>
#include <thread>
#include <wx/stdpaths.h>
#include <wx/dir.h>
#include <wx/evtloop.h>

#include "GS.h"
#include "options_tools.h"
#include "retro_messager.h"
#include "language_injector.h"
#include "input.h"
#include "svnrev.h"
#include "disk_control.h"
#include "SPU2/Global.h"
#include "ps2/BiosTools.h"
#include "memcard_retro.h"



#include "MTVU.h"

#ifdef PERF_TEST
static struct retro_perf_callback perf_cb;

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

retro_environment_t environ_cb;
retro_video_refresh_t video_cb;
struct retro_hw_render_callback hw_render;
static ConsoleColors log_color = Color_Default;
unsigned libretro_msg_interface_version = 0;
retro_log_printf_t log_cb;

// renderswitch - tells GSdx to go into dx9 sw if "renderswitch" is set.
bool renderswitch = false;
uint renderswitch_delay = 0;
Pcsx2App* pcsx2;
static wxFileName bios_dir;

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
	enum retro_pixel_format xrgb888 = RETRO_PIXEL_FORMAT_XRGB8888;
	environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);
	struct retro_log_callback log;
	if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
	{
		log_cb = log.log;
	}
	environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &libretro_msg_interface_version);

	const char* system = nullptr;
	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system);
	

	
	const char* save_dir = nullptr;
	environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir);

	// checks and create save folders
	const char* pcsx2_save_dir = MemCardRetro::checkSaveDir(save_dir, "//pcsx2");
	const char* shared_memcards_dir = MemCardRetro::checkSaveDir(pcsx2_save_dir, "//shared_memcards");

	// creates shared_memcards if not existing
	for (int i = 0; i < 9; i++) 
	{
		const char* path_to_memcard_file = nullptr;
		const char* memcard_filename = nullptr;

		memcard_filename = MemCardRetro::GetSharedMemCardFileName("Shared Memory Card", i + 1);
		path_to_memcard_file = MemCardRetro::CreateMemCardFilePath(shared_memcards_dir, memcard_filename);

		if (!MemCardRetro::isMemCardExisting(path_to_memcard_file))
		{
			MemCardRetro::Create(path_to_memcard_file);
			log_cb(RETRO_LOG_DEBUG, "creating shared memcard:\n");
			log_cb(RETRO_LOG_DEBUG, path_to_memcard_file);
		}
	}

	// get available shared memcards and fill the options

	wxArrayString shared_memcard_list;
	wxString shared_path = wxString(shared_memcards_dir);
	
	wxDir::GetAllFiles(shared_path, &shared_memcard_list, L"*.*", wxDIR_FILES);
	static std::vector<std::string> memcard_list;
	for (wxString memcard_file : shared_memcard_list)
	{
		wxString description;
		if (MemCardRetro::IsAMemoryCard(memcard_file)) {
			std::string file_n = MemCardRetro::getMemCardFileRawName(memcard_file);
			memcard_list.push_back((std::string)memcard_file);
			memcard_list.push_back(file_n);

		}
	}


	// breaking after filled should be more quick than looping throught all options
	// so we do this 2 times for both memcard options slot.
	// the max number of shared memcard is limited to 20
	for (retro_core_option_definition& def : option_defs)
	{								
		if (!def.key || strcmp(def.key, "pcsx2_memcard_slot_1")) continue; // || strcmp(def.key, "pcsx2_memcard_slot_2")
		size_t i = 0, numfiles = memcard_list.size();
		int cont = 0;
		def.values[i++] = { "Empty", "Empty" };

		for (size_t f = 0; f != numfiles; f += 2)
		{
			def.values[i++] = { memcard_list[f].c_str(), memcard_list[f + 1].c_str() };
			cont++;
			if (cont >= 20) break;
		}

		def.values[i++] = { NULL, NULL };
		def.default_value = def.values[0].value;
		break;
	}

	for (retro_core_option_definition& def : option_defs)
	{
		if (!def.key || strcmp(def.key, "pcsx2_memcard_slot_2")) continue; 
		size_t i = 0, numfiles = memcard_list.size();
		int cont = 0;
		def.values[i++] = { "Empty", "Empty" };

		for (size_t f = 0; f != numfiles; f += 2)
		{
			def.values[i++] = { memcard_list[f].c_str(), memcard_list[f + 1].c_str() };
			cont++;
			if (cont >= 20) break;
		}

		def.values[i++] = { NULL, NULL };
		def.default_value = def.values[0].value;
		break;
	}



	// instantiate the pcsx2 app and so some things on it

	//pcsx2 = new Pcsx2App;
	//wxApp::SetInstance(pcsx2);
	pcsx2 = &wxGetApp();
	#if 0
		int argc = 0;
		pcsx2->Initialize(argc, (wchar_t**)nullptr);
		wxModule::RegisterModules();
		wxModule::InitializeModules();
	#endif
	InitCPUTicks();
	pxDoOutOfMemory = SysOutOfMemory_EmergencyResponse;
	g_Conf = std::make_unique<AppConfig>();
	
	
	// get the BIOS available and fill the option

	bios_dir = Path::Combine(system, "pcsx2/bios");

	wxArrayString bios_list;
	wxDir::GetAllFiles(bios_dir.GetFullPath(), &bios_list, L"*.*", wxDIR_FILES);
	static std::vector<std::string> bios_files;
	for (wxString bios_file : bios_list)
	{
			wxString description;
			if (IsBIOS(bios_file, description)) {
				bios_files.push_back((std::string)bios_file);
				bios_files.push_back((std::string)description);
			}
	}

	for (retro_core_option_definition& def : option_defs)
	{
		if (!def.key || strcmp(def.key, "pcsx2_bios")) continue;
		size_t i = 0, numfiles = bios_files.size();
		int cont = 0;
		for (size_t f = 0; f != numfiles; f += 2)
		{

			def.values[i++] = { bios_files[f].c_str(), bios_files[f + 1].c_str() };
			cont++;
		}
		def.values[cont] = { NULL, NULL };
		def.default_value = def.values[0].value;
		break;
	}

	// loads the options structure to the frontend

	libretro_set_core_options(environ_cb);

	// some other stuffs about pcsx2

	pcsx2->DetectCpuAndUserMode();
	pcsx2->AllocateCoreStuffs();

	// apply options to pcsx2

	if (!option_value(BOOL_PCSX2_OPT_ENABLE_SPEEDHACKS, KeyOptionBool::return_type))
		g_Conf->PresetIndex = 1;

	g_Conf->BaseFilenames.Plugins[PluginId_GS] = "Built-in";
	g_Conf->BaseFilenames.Plugins[PluginId_PAD] = "Built-in";
	g_Conf->BaseFilenames.Plugins[PluginId_USB] = "Built-in";
	g_Conf->BaseFilenames.Plugins[PluginId_DEV9] = "Built-in";
	g_Conf->EmuOptions.EnableIPC = false;

	g_Conf->EmuOptions.EnableWideScreenPatches = option_value(BOOL_PCSX2_OPT_ENABLE_WIDESCREEN_PATCHES, KeyOptionBool::return_type);
	g_Conf->EmuOptions.GS.FrameSkipEnable = option_value(BOOL_PCSX2_OPT_FRAMESKIP, KeyOptionBool::return_type);
	g_Conf->EmuOptions.GS.FramesToDraw = option_value(INT_PCSX2_OPT_FRAMES_TO_DRAW, KeyOptionInt::return_type);
	g_Conf->EmuOptions.GS.FramesToSkip = option_value(INT_PCSX2_OPT_FRAMES_TO_SKIP, KeyOptionInt::return_type);

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

void retro_deinit(void)
{
	vu1Thread.Cancel();
	pcsx2->CleanupOnExit();
	pcsx2->OnExit();
#ifdef PERF_TEST
	perf_cb.perf_log();
#endif
}

void retro_get_system_info(retro_system_info* info)
{
#ifdef GIT_REV
	info->library_version = GIT_REV;
#else
	static char version[] = "#.#.#";
	version[0] = '0' + PCSX2_VersionHi;
	version[2] = '0' + PCSX2_VersionMid;
	version[4] = '0' + PCSX2_VersionLo;
	info->library_version = version;
#endif

	info->library_name = "pcsx2 (alpha)";
	info->valid_extensions = "elf|iso|ciso|chd|cso|cue|bin|m3u";
	info->need_fullpath = true;
	info->block_extract = true;
}

void retro_get_system_av_info(retro_system_av_info* info)
{
	if ( !std::strcmp(option_value(STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type), "Software") || !std::strcmp(option_value(STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type), "Null"))
	{
		info->geometry.base_width = 640;
		info->geometry.base_height = 448;
	}
	else
	{
		info->geometry.base_width = 640 * option_value(INT_PCSX2_OPT_UPSCALE_MULTIPLIER, KeyOptionInt::return_type);
		info->geometry.base_height = 448 * option_value(INT_PCSX2_OPT_UPSCALE_MULTIPLIER, KeyOptionInt::return_type);
	}

	info->geometry.max_width = info->geometry.base_width;
	info->geometry.max_height = info->geometry.base_height;

	if (option_value(INT_PCSX2_OPT_ASPECT_RATIO, KeyOptionInt::return_type) == 0)
		info->geometry.aspect_ratio = 4.0f / 3.0f;
	else
		info->geometry.aspect_ratio = 16.0f / 9.0f;
	info->timing.fps = (retro_get_region() == RETRO_REGION_NTSC) ? (60.0f / 1.001f) : 50.0f;
	info->timing.sample_rate = 48000;
}

void retro_reset(void)
{
	GetMTGS().FinishTaskInThread();
	GetCoreThread().ResetQuick();
	DiskControl::eject_state = false;
}

static void context_reset(void)
{
	GetMTGS().OpenPlugin();
}

static void context_destroy(void)
{
	GetMTGS().FinishTaskInThread();

	while (pcsx2->HasPendingEvents())
		pcsx2->ProcessPendingEvents();
	GetMTGS().ClosePlugin();
	while (pcsx2->HasPendingEvents())
		pcsx2->ProcessPendingEvents();
}

static bool set_hw_render(retro_hw_context_type type)
{

	hw_render.context_type = type;
	hw_render.context_reset = context_reset;
	hw_render.context_destroy = context_destroy;
	hw_render.bottom_left_origin = true;
	hw_render.depth = true;
	hw_render.cache_context = false;

	switch (type)
	{
		case RETRO_HW_CONTEXT_DIRECT3D:
			hw_render.version_major = 11;
			hw_render.version_minor = 0;
			hw_render.cache_context = true;
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
			hw_render.cache_context = true;
			break;

		case RETRO_HW_CONTEXT_OPENGLES3:
			if (set_hw_render(RETRO_HW_CONTEXT_OPENGL))
				return true;

			hw_render.version_major = 3;
			hw_render.version_minor = 0;
			hw_render.cache_context = true;
			break;

		case RETRO_HW_CONTEXT_NONE:
			hw_render.version_major = 3;
			hw_render.version_minor = 0;
			hw_render.cache_context = true;
			break;

		default:
			return false;
	}

	return environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render);
}

static wxVector<wxString>
read_m3u_file(const wxFileName& m3u_file)
{
	log_cb(RETRO_LOG_DEBUG, "Reading M3U file");

	wxVector<wxString> result;
	wxVector<wxString> nonexistent;

	wxTextFile m3u_data;
	if (!m3u_data.Open(m3u_file.GetFullPath()))
	{
		log_cb(RETRO_LOG_ERROR, "M3U file \"%s\" cannot be read", m3u_file.GetFullPath().c_str());
		return result;
	}

	wxString base_path = m3u_file.GetPath();

	// This is the UTF-8 representation of U+FEFF.
	const wxString utf8_bom = "\xEF\xBB\xBF";

	wxString line = m3u_data.GetFirstLine();
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
				log_cb(RETRO_LOG_DEBUG, "Found disc image in M3U file, %s", discFile.GetFullPath());
				result.push_back(discFile.GetFullPath());
			}
			else
			{
				wxString full_path = discFile.GetFullPath();
				nonexistent.push_back(full_path);
				log_cb(RETRO_LOG_WARN, "File specified in the M3U file \"%s\" was not found:\n%s", m3u_file.GetFullPath().c_str(), full_path.c_str());
			}
		}
	}

	if (result.empty())
		log_cb(RETRO_LOG_ERROR, "No paths found in the M3U file \"%s\"", m3u_file.GetFullPath().c_str());

	return result;
}

bool retro_load_game(const struct retro_game_info* game)
{

	const char* selected_bios = option_value(STRING_PCSX2_OPT_BIOS, KeyOptionString::return_type);
	if (selected_bios == NULL)
	{
		log_cb(RETRO_LOG_ERROR, "Could not find any valid PS2 Bios File in %s\n", (const char*)bios_dir.GetFullPath());
		return false;
	}
	else
		log_cb(RETRO_LOG_INFO, "Loading selected BIOS:  %s\n", selected_bios);
	

	const char* system = nullptr;
	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system);

	//	pcsx2->Overrides.Gamefixes.Set( id, true);

	// By default no IRX injection
	g_Conf->CurrentIRX = "";
	g_Conf->BaseFilenames.Bios = selected_bios;

	DiskControl::eject_state = false;

	// check what memcard to use
	const char* path_to_memcard_file_slot1 = nullptr;

	



	if (game)
	{

		LanguageInjector::Inject(
			(std::string)option_value(STRING_PCSX2_OPT_BIOS, KeyOptionString::return_type),
			option_value(STRING_PCSX2_OPT_SYSTEM_LANGUAGE, KeyOptionString::return_type)
		);

		wxVector<wxString> game_paths;

		wxFileName file_name(game->path);
		if (file_name.GetExt() == "m3u")
		{
			game_paths = read_m3u_file(file_name);
			log_cb(RETRO_LOG_DEBUG, "Found %u game images in M3U file.", game_paths.size());
		}
		else
		{
			game_paths.push_back(game->path);
		}

		DiskControl::disk_images.assign(game_paths.begin(), game_paths.end());

		u32 magic = 0;
		FILE* fp = fopen(game_paths[0], "rb");

		if (!fp)
		{
			log_cb(RETRO_LOG_ERROR, "Could not open File: %s\n", game_paths[0]);
			return false;
		}

		fread(&magic, 4, 1, fp);
		fclose(fp);

		if (magic == 0x464C457F) // elf
		{
			// g_Conf->CurrentIRX = "";
			log_cb(RETRO_LOG_INFO, "ELF file detected, loading content....\n");
			g_Conf->EmuOptions.UseBOOT2Injection = true;
			pcsx2->SysExecute(CDVD_SourceType::NoDisc, game_paths[0]);
		}
		else
		{	
			
			g_Conf->EmuOptions.UseBOOT2Injection = option_value(BOOL_PCSX2_OPT_FASTBOOT, KeyOptionBool::return_type);
			g_Conf->CdvdSource = CDVD_SourceType::Iso;
			g_Conf->CurrentIso = game_paths[0];
			
			
			if (!option_value(BOOL_PCSX2_OPT_MEMCARD_LEGACY, KeyOptionBool::return_type)) {

				if (strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type), "Empty") == 0 && 
					strcmp(option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type), "Empty") == 0)
				{
					// per-game memory card
					const char* save_dir = nullptr;
					environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir);

					log_cb(RETRO_LOG_DEBUG, "save directory: %s\n", save_dir);

					const char* memcard_content_path = MemCardRetro::checkSaveDir(save_dir, "//pcsx2");
					const char* memcard_filename = MemCardRetro::getMemCardFileName(game_paths[0]);
					path_to_memcard_file_slot1 = MemCardRetro::CreateMemCardFilePath(memcard_content_path, memcard_filename);
					RetroMessager::Notification("Memory Card Mode: Per-game", true);

					if (!MemCardRetro::isMemCardExisting(path_to_memcard_file_slot1))
					{
						if (MemCardRetro::Create(path_to_memcard_file_slot1))
							RetroMessager::Notification("Memory Card created for the content", true);
					}

					wxString MemCardfileNameSlot1 = wxString(path_to_memcard_file_slot1);
					// insert the memcard
					g_Conf->Mcd[0].Enabled = true;
					g_Conf->Mcd[0].Filename = MemCardfileNameSlot1;

					std::string card_used = (std::string)MemCardfileNameSlot1;
					log_cb(RETRO_LOG_DEBUG, "Memory card on slot 1: %s\n", card_used.c_str());


				}
				else
				{
					// shared-memcards
					RetroMessager::Notification("Memory Card Mode: Shared", true);
					const char* memcard1 = option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_1, KeyOptionString::return_type);
					const char* memcard2 = option_value(STRING_PCSX2_OPT_MEMCARD_SLOT_2, KeyOptionString::return_type);

					if (strcmp(memcard1, "Empty") == 0)
					{
						g_Conf->Mcd[0].Enabled = false;
						
					}
					else {
						g_Conf->Mcd[0].Enabled = true;
						g_Conf->Mcd[0].Filename = wxString(memcard1);
						std::string card_used = (std::string)memcard1;
						log_cb(RETRO_LOG_DEBUG, "Memory card on slot 1: %s\n", card_used.c_str());
					}

					if (strcmp(memcard2, "Empty") == 0)
					{
						g_Conf->Mcd[1].Enabled = false;
						
					}
					else {
						// if memcard in slot 2 and 1  are the same, we disable slot2
						if (strcmp(memcard1, memcard2) == 0) {
							g_Conf->Mcd[1].Enabled = false;
							RetroMessager::Notification("Detected same memory card on both slots, disabled slot 2", true);
						}
						else
						{
							g_Conf->Mcd[1].Enabled = true;
							g_Conf->Mcd[1].Filename = wxString(memcard2);
							std::string card_used = (std::string)memcard2;
							log_cb(RETRO_LOG_DEBUG, "Memory card on slot 2: %s\n", card_used.c_str());
						}
						
					}

				}


			}else{

				// memcard legacy, the old location system/pcsx2/memcards
				wxString MemCardfileNameSlot1 = wxString(MemCardRetro::GetDefaultMemoryCardPath1(system));
				wxString MemCardfileNameSlot2 = wxString(MemCardRetro::GetDefaultMemoryCardPath2(system));
				g_Conf->Mcd[0].Enabled = true;
				g_Conf->Mcd[0].Filename = MemCardfileNameSlot1;
				g_Conf->Mcd[1].Enabled = true;
				g_Conf->Mcd[1].Filename = MemCardfileNameSlot2;
				std::string card_used = (std::string)MemCardfileNameSlot1;
				log_cb(RETRO_LOG_DEBUG, "Using memory card: %s\n", card_used.c_str());
				RetroMessager::Notification("Memory Card mode: Legacy");
			
			}



			if (!option_value(BOOL_PCSX2_OPT_MEMCARD_LEGACY, KeyOptionBool::return_type)) {

			
				//std::string slot_msg = "Using Memory card SLOT " + std::to_string(save_slot);
				//RetroMessager::Notification(slot_msg.c_str(), true);
			}
			else
			{

			}
			
			if (!option_value(BOOL_PCSX2_OPT_BOOT_TO_BIOS, KeyOptionBool::return_type))
			{
				pcsx2->SysExecute(g_Conf->CdvdSource);
				log_cb(RETRO_LOG_INFO, "Game Loaded\n");
			}
			else
			{
				// we enter here in the bios, so we have the correct memcards loaded
				log_cb(RETRO_LOG_INFO, "Entrerning BIOS Menu.....\n");
				RetroMessager::Notification("Boot to BIOS", true);
				g_Conf->EmuOptions.UseBOOT2Injection = option_value(BOOL_PCSX2_OPT_FASTBOOT, KeyOptionBool::return_type);
				g_Conf->CdvdSource = CDVD_SourceType::NoDisc;
				g_Conf->CurrentIso = "";
				pcsx2->SysExecute(g_Conf->CdvdSource);

			}
		}
		
	}
	else
	{
		log_cb(RETRO_LOG_INFO, "Entrerning BIOS Menu.....\n");
		g_Conf->EmuOptions.UseBOOT2Injection = option_value(BOOL_PCSX2_OPT_FASTBOOT, KeyOptionBool::return_type);
		g_Conf->CdvdSource = CDVD_SourceType::NoDisc;
		g_Conf->CurrentIso = "";
		pcsx2->SysExecute(g_Conf->CdvdSource);
	}

	g_Conf->EmuOptions.GS.VsyncEnable = VsyncMode::Off;
	g_Conf->EmuOptions.GS.FramesToDraw = 1;
	//	g_Conf->CurrentGameArgs = "";
	g_Conf->EmuOptions.GS.FrameLimitEnable = false;
	//	g_Conf->EmuOptions.GS.SynchronousMTGS = true;

	Input::Init();
	Input::RumbleEnabled(
		option_value(BOOL_PCSX2_OPT_GAMEPAD_RUMBLE_ENABLE, KeyOptionBool::return_type),
		option_value(INT_PCSX2_OPT_GAMEPAD_RUMBLE_FORCE, KeyOptionInt::return_type)
		);

	retro_hw_context_type context_type = RETRO_HW_CONTEXT_OPENGL;
	const char* option_renderer = option_value(STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type);
	log_cb(RETRO_LOG_INFO, "options renderer: %s\n", option_renderer);

	if (! std::strcmp(option_renderer,"Auto"))
		environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &context_type);
#ifdef _WIN32
	else if (!std::strcmp(option_renderer, "D3D11"))
		context_type = RETRO_HW_CONTEXT_DIRECT3D;
#endif
	else if (!std::strcmp(option_renderer, "Null"))
		context_type = RETRO_HW_CONTEXT_NONE;

	return set_hw_render(context_type);
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info,
							 size_t num_info)
{
	return false;
}

void retro_unload_game(void)
{
	//	GetMTGS().FinishTaskInThread();
	//		GetMTGS().ClosePlugin();
	GetMTGS().FinishTaskInThread();

	while (pcsx2->HasPendingEvents())
		pcsx2->ProcessPendingEvents();
	GetMTGS().ClosePlugin();

	while (pcsx2->HasPendingEvents())
		pcsx2->ProcessPendingEvents();
}


void retro_run(void)
{
	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
	{
		log_cb(RETRO_LOG_INFO, "Options Change detected...\n");
		SetGSConfig().FrameSkipEnable = option_value(BOOL_PCSX2_OPT_FRAMESKIP, KeyOptionBool::return_type);
		SetGSConfig().FramesToDraw = option_value(INT_PCSX2_OPT_FRAMES_TO_DRAW, KeyOptionInt::return_type);
		SetGSConfig().FramesToSkip = option_value(INT_PCSX2_OPT_FRAMES_TO_SKIP, KeyOptionInt::return_type);
		GSUpdateOptions();
		Input::RumbleEnabled(
			option_value(BOOL_PCSX2_OPT_GAMEPAD_RUMBLE_ENABLE, KeyOptionBool::return_type),
			option_value(INT_PCSX2_OPT_GAMEPAD_RUMBLE_FORCE, KeyOptionInt::return_type)
		);
	}

	Input::Update();

	RETRO_PERFORMANCE_INIT(pcsx2_run);
	RETRO_PERFORMANCE_START(pcsx2_run);

	GetMTGS().ExecuteTaskInThread();

	RETRO_PERFORMANCE_STOP(pcsx2_run);
}

size_t retro_serialize_size(void)
{
	return 0;
}

bool retro_serialize(void* data, size_t size)
{
	return false;
}
bool retro_unserialize(const void* data, size_t size)
{
	return false;
}

unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

unsigned retro_api_version()
{
	return RETRO_API_VERSION;
}

size_t retro_get_memory_size(unsigned id)
{
	return 0;
}

void* retro_get_memory_data(unsigned id)
{
	return NULL;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
}

int Interpolation = 4;
bool EffectsDisabled = false;
bool postprocess_filter_dealias = false;
unsigned int delayCycles = 4;

static retro_audio_sample_batch_t batch_cb;
static retro_audio_sample_t sample_cb;
static int write_pos = 0;

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	batch_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	sample_cb = cb;
}


void SndBuffer::Write(const StereoOut32& Sample)
{
#if 0
	static s16 snd_buffer[0x100 << 1];
	snd_buffer[write_pos++] = Sample.Left >> 12;
	snd_buffer[write_pos++] = Sample.Right >> 12;
	if(write_pos == (0x100 << 1))
	{
		batch_cb(snd_buffer, write_pos >> 1);
		write_pos = 0;
	}
#else
	sample_cb(Sample.Left >> 12, Sample.Right >> 12);
#endif
}

void SndBuffer::Init()
{
	write_pos = 0;
}

void SndBuffer::Cleanup()
{
}

s32 SndBuffer::Test()
{
	return 0;
}

void SndBuffer::ClearContents()
{
}

void DspUpdate()
{
}

s32 DspLoadLibrary(wchar_t* fileName, int modnum)
{
	return 0;
}

void ReadSettings()
{
}
#ifndef _WIN32
void SysMessage(const char* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	vprintf(fmt, list);
	va_end(list);
}
#endif
wxEventLoopBase* Pcsx2AppTraits::CreateEventLoop()
{
	return new wxEventLoop();
	//	 return new wxGUIEventLoop();
	//	 return new wxConsoleEventLoop();
}

#ifdef wxUSE_STDPATHS
class Pcsx2StandardPaths : public wxStandardPaths
{
public:
	virtual wxString GetExecutablePath() const
	{
		const char* system = nullptr;
		environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system);
		return Path::Combine(system, "pcsx2/PCSX2");
	}
	wxString GetResourcesDir() const
	{
		const char* system = nullptr;
		environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system);
		return Path::Combine(system, "pcsx2/Langs");
	}
	wxString GetUserLocalDataDir() const
	{
		const char* savedir = nullptr;
		environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &savedir);
		return Path::Combine(savedir, "pcsx2");
	}
};

wxStandardPaths& Pcsx2AppTraits::GetStandardPaths()
{
	static Pcsx2StandardPaths stdPaths;
	return stdPaths;
}
#endif
