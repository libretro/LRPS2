
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
#include "input.h"
#include "svnrev.h"
#include "SPU2/Global.h"
#include "ps2/BiosTools.h"
#include "CDVD/CDVD.h"
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
static retro_log_printf_t log_cb;

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

static void RetroLog_DoSetColor(ConsoleColors color)
{
	if (color != Color_Current)
		log_color = color;
}

static void RetroLog_DoWrite(const wxString& fmt)
{
	retro_log_level level = RETRO_LOG_INFO;
	switch (log_color)
	{
		case Color_StrongRed: // intended for errors
			level = RETRO_LOG_ERROR;
			break;
		case Color_StrongOrange: // intended for warnings
			level = RETRO_LOG_WARN;
			break;
		case Color_Cyan:   // faint visibility, intended for logging PS2/IOP output
		case Color_Yellow: // faint visibility, intended for logging PS2/IOP output
		case Color_White:  // faint visibility, intended for logging PS2/IOP output
			level = RETRO_LOG_DEBUG;
			break;
		default:
		case Color_Default:
		case Color_Black:
		case Color_Green:
		case Color_Red:
		case Color_Blue:
		case Color_Magenta:
		case Color_Orange:
		case Color_Gray:
		case Color_StrongBlack:
		case Color_StrongGreen: // intended for infrequent state information
		case Color_StrongBlue:  // intended for block headings
		case Color_StrongMagenta:
		case Color_StrongGray:
		case Color_StrongCyan:
		case Color_StrongYellow:
		case Color_StrongWhite:
			break;
	}

	log_cb(level, "%s", (const char*)fmt);
}

static void RetroLog_SetTitle(const wxString& title)
{
	RetroLog_DoWrite(title + L"\n");
}

static void RetroLog_Newline()
{
	//	RetroLog_DoWrite(L"\n");
}

static void RetroLog_DoWriteLn(const wxString& fmt)
{
	RetroLog_DoWrite(fmt + L"\n");
}

static const IConsoleWriter ConsoleWriter_Libretro =
	{
		RetroLog_DoWrite,
		RetroLog_DoWriteLn,
		RetroLog_DoSetColor,

		RetroLog_DoWrite,
		RetroLog_Newline,
		RetroLog_SetTitle,

		0, // instance-level indentation (should always be 0)
};

static wxVector<wxString> disk_images;
static int image_index = 0;
static bool eject_state;
static bool RETRO_CALLCONV set_eject_state(bool ejected)
{
	if (eject_state == ejected)
		return false;

	eject_state = ejected;

	GetMTGS().FinishTaskInThread();
	CoreThread.Pause();

	if (ejected)
		cdvdCtrlTrayOpen();
	else
	{
		if (image_index < 0 || image_index >= (int)disk_images.size())
			g_Conf->CdvdSource = CDVD_SourceType::NoDisc;
		else
		{
			g_Conf->CurrentIso = disk_images[image_index];
			g_Conf->CdvdSource = CDVD_SourceType::Iso;
			CDVDsys_SetFile(CDVD_SourceType::Iso, g_Conf->CurrentIso);
		}
		cdvdCtrlTrayClose();
	}

	CoreThread.Resume();
	return true;
}
static bool RETRO_CALLCONV get_eject_state(void)
{
	return eject_state;
}

static unsigned RETRO_CALLCONV get_image_index(void)
{
	return image_index;
}
static bool RETRO_CALLCONV set_image_index(unsigned index)
{
	if (eject_state)
		image_index = index;

	return eject_state;
}
static unsigned RETRO_CALLCONV get_num_images(void)
{
	return disk_images.size();
}

static bool RETRO_CALLCONV replace_image_index(unsigned index, const struct retro_game_info* info)
{
	if (index >= disk_images.size())
		return false;

	if (!info->path)
	{
		disk_images.erase(disk_images.begin() + index);
		if (!disk_images.size())
			image_index = -1;
		else if (image_index > (int)index)
			image_index--;
	}
	else
		disk_images[index] = info->path;

	return true;
}

static bool RETRO_CALLCONV add_image_index(void)
{
	disk_images.push_back("");
	return true;
}

static bool RETRO_CALLCONV set_initial_image(unsigned index, const char* path)
{
	if (index >= disk_images.size())
		index = 0;

	image_index = index;

	return true;
}

static bool RETRO_CALLCONV get_image_path(unsigned index, char* path, size_t len)
{
	if (index >= disk_images.size())
		return false;

	if (disk_images[index].empty())
		return false;

	strncpy(path, disk_images[index].c_str(), len);
	return true;
}
static bool RETRO_CALLCONV get_image_label(unsigned index, char* label, size_t len)
{
	if (index >= disk_images.size())
		return false;

	if (disk_images[index].empty())
		return false;

	strncpy(label, disk_images[index].c_str(), len);
	return true;
}

void retro_init(void)
{
	enum retro_pixel_format xrgb888 = RETRO_PIXEL_FORMAT_XRGB8888;
	environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);
	struct retro_log_callback log;
	if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
	{
		log_cb = log.log;
#if 0
		Console_SetActiveHandler(ConsoleWriter_Libretro);
#endif
	}

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
	pcsx2->DetectCpuAndUserMode();
	pcsx2->AllocateCoreStuffs();
	//	pcsx2->GetGameDatabase();

	g_Conf->BaseFilenames.Plugins[PluginId_GS] = "Built-in";
	g_Conf->BaseFilenames.Plugins[PluginId_PAD] = "Built-in";
	g_Conf->BaseFilenames.Plugins[PluginId_USB] = "Built-in";
	g_Conf->BaseFilenames.Plugins[PluginId_DEV9] = "Built-in";
	g_Conf->EmuOptions.EnableIPC = false;

	const char* system = nullptr;
	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system);
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

	for (retro_core_option_definition& def : option_defs_us)
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

	libretro_set_core_options(environ_cb);

	static retro_disk_control_ext_callback disk_control = {
		set_eject_state,
		get_eject_state,
		get_image_index,
		set_image_index,
		get_num_images,
		replace_image_index,
		add_image_index,
		set_initial_image,
		get_image_path,
		get_image_label,
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
	info->valid_extensions = "elf|iso|ciso|cso|cue|bin|m3u";
	info->need_fullpath = true;
	info->block_extract = true;
}

void retro_get_system_av_info(retro_system_av_info* info)
{
	if ( option_value(environ_cb, STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type) == "Software" || option_value(environ_cb, STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type) == "Null")
	{
		info->geometry.base_width = 640;
		info->geometry.base_height = 448;
	}
	else
	{
		info->geometry.base_width = 640 * option_value(environ_cb, INT_PCSX2_OPT_UPSCALE_MULTIPLIER, KeyOptionInt::return_type);
		info->geometry.base_height = 448 * option_value(environ_cb, INT_PCSX2_OPT_UPSCALE_MULTIPLIER, KeyOptionInt::return_type);
	}

	info->geometry.max_width = info->geometry.base_width;
	info->geometry.max_height = info->geometry.base_height;

	if (! option_value(environ_cb, BOOL_PCSX2_OPT_FORCE_WIDESCREEN, KeyOptionBool::return_type))
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
	eject_state = false;
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
			return true;

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
	const char* selected_bios = option_value(environ_cb, STRING_PCSX2_OPT_BIOS, KeyOptionString::return_type);
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

	eject_state = false;

	if (game)
	{
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

		disk_images.assign(game_paths.begin(), game_paths.end());

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
			log_cb(RETRO_LOG_INFO, "Something went wrong while loading game");
			g_Conf->EmuOptions.UseBOOT2Injection = true;
			pcsx2->SysExecute(CDVD_SourceType::NoDisc, game_paths[0]);
		}
		else
		{	
			/*
			if (option_value(environ_cb, BOOL_PCSX2_OPT_ENABLE_SPEEDHACKS, KeyOptionBool::return_type))
			{
				g_Conf->EnablePresets = true;
				g_Conf->PresetIndex = 1; //option_value(environ_cb, INT_PCSX2_OPT_SPEEDHACKS_PRESET, KeyOptionInt::return_type);
			}
			else
				g_Conf->EnablePresets = false;
			*/

			g_Conf->EmuOptions.UseBOOT2Injection = option_value(environ_cb, BOOL_PCSX2_OPT_FASTBOOT, KeyOptionBool::return_type);
			g_Conf->CdvdSource = CDVD_SourceType::Iso;
			g_Conf->CurrentIso = game_paths[0];
			pcsx2->SysExecute(g_Conf->CdvdSource);
			log_cb(RETRO_LOG_INFO, "Game Loaded");
		}
	}
	else
	{
		log_cb(RETRO_LOG_INFO, "Entrerning BIOS Menu.....");
		g_Conf->EmuOptions.UseBOOT2Injection = option_value(environ_cb, BOOL_PCSX2_OPT_FASTBOOT, KeyOptionBool::return_type);
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

	retro_hw_context_type context_type = RETRO_HW_CONTEXT_OPENGL;
	if (option_value(environ_cb, STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type) == "Auto")
		environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &context_type);
#ifdef _WIN32
	else if (option_value(environ_cb, STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type) == "D3D11")
		context_type = RETRO_HW_CONTEXT_DIRECT3D;
#endif
	else if (option_value(environ_cb, STRING_PCSX2_OPT_RENDERER, KeyOptionString::return_type) == "Null")
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
	SetGSConfig().FrameSkipEnable = option_value(environ_cb, BOOL_PCSX2_OPT_FRAMESKIP, KeyOptionBool::return_type);
	SetGSConfig().FramesToDraw = option_value(environ_cb, INT_PCSX2_OPT_FRAMES_TO_DRAW, KeyOptionInt::return_type);
	SetGSConfig().FramesToSkip = option_value(environ_cb, INT_PCSX2_OPT_FRAMES_TO_SKIP, KeyOptionInt::return_type);

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
