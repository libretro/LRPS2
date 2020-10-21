#include "PrecompiledHeader.h"

#include "PrecompiledHeader.h"
#include "AppCommon.h"
#include "App.h"

#include <cstdint>
#include <libretro.h>
#include <string>
#include <thread>
#include <wx/stdpaths.h>

#include "GS.h"
#include "options.h"
#include "input.h"
#include "svnrev.h"
#include "SPU2/Global.h"

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

namespace Options
{
static Option test("pcsx2_test", "Test", false);
}

// renderswitch - tells GSdx to go into dx9 sw if "renderswitch" is set.
bool renderswitch = false;
uint renderswitch_delay = 0;
static Pcsx2App* pcsx2;

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;
	Options::SetVariables();
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

const IConsoleWriter ConsoleWriter_Libretro =
	{
		RetroLog_DoWrite,
		RetroLog_DoWriteLn,
		RetroLog_DoSetColor,

		RetroLog_DoWrite,
		RetroLog_Newline,
		RetroLog_SetTitle,

		0, // instance-level indentation (should always be 0)
};

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

	pcsx2 = new Pcsx2App;
	wxApp::SetInstance(pcsx2);
#if 0
	int argc = 0;
	pcsx2->Initialize(argc, (wchar_t**)nullptr);
	wxModule::RegisterModules();
	wxModule::InitializeModules();
#endif

	InitCPUTicks();
	pxDoAssert = AppDoAssert;
	pxDoOutOfMemory = SysOutOfMemory_EmergencyResponse;
	g_Conf = std::make_unique<AppConfig>();
	i18n_SetLanguage(wxLANGUAGE_DEFAULT);
	i18n_SetLanguagePath();
	pcsx2->DetectCpuAndUserMode();
	pcsx2->AllocateCoreStuffs();
	//	pcsx2->GetGameDatabase();
}

void retro_deinit(void)
{
	pcsx2->CleanupOnExit();
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

	info->library_name = "pcsx2";
	info->valid_extensions = "elf|iso|ciso|cue|bin";
	info->need_fullpath = true;
	info->block_extract = true;
}

void retro_get_system_av_info(retro_system_av_info* info)
{
	//	info->geometry.base_width = 640;
	//	info->geometry.base_height = 480;
	info->geometry.base_width = 1280;
	info->geometry.base_height = 896;

	info->geometry.max_width = info->geometry.base_width;
	//	info->geometry.max_height = 1280;
	info->geometry.max_height = info->geometry.base_height;

	info->geometry.aspect_ratio = 4.0 / 3.0;
	info->timing.fps = (retro_get_region() == RETRO_REGION_NTSC) ? (60.0f / 1.001f) : 50.0f;
	info->timing.sample_rate = 48000;
}

void retro_reset(void)
{
}

static void context_reset()
{
	printf("Context reset\n");
	GetCoreThread().Resume();
	GetMTGS().OpenPlugin();
	//	GSsetVsync(0);
}
static void context_destroy()
{
	GetMTGS().FinishTaskInThread();
	GetMTGS().ClosePlugin();
	GetCoreThread().Suspend(true);
	printf("Context destroy\n");
}

bool retro_load_game(const struct retro_game_info* game)
{
	const char* system = nullptr;
	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system);

	//	pcsx2->Overrides.SettingsFolder = "";
	//	pcsx2->Overrides.Gamefixes.Set( id, true);

	// By default no IRX injection
	g_Conf->CurrentIRX = "";
	g_Conf->EmuOptions.UseBOOT2Injection = true; // fastboot

	if (game)
	{
		u32 magic = 0;
		FILE* fp = fopen(game->path, "rb");
		if (!fp)
			return false;

		fread(&magic, 4, 1, fp);
		fclose(fp);

		if (magic == 0x464C457F) // elf
		{
			// g_Conf->CurrentIRX = "";
			pcsx2->SysExecute(CDVD_SourceType::NoDisc, game->path);
		}
		else
		{
			g_Conf->CdvdSource = CDVD_SourceType::Iso;
			g_Conf->CurrentIso = game->path;
			pcsx2->SysExecute(CDVD_SourceType::Iso);
		}
	}
	else
	{
		pcsx2->Startup.CdvdSource = CDVD_SourceType::NoDisc;
		pcsx2->Startup.SysAutoRun = true;
		g_Conf->CdvdSource = CDVD_SourceType::NoDisc;
		pcsx2->SysExecute(CDVD_SourceType::NoDisc);
	}

	g_Conf->CurrentGameArgs = "";
	g_Conf->EmuOptions.GS.FrameLimitEnable = false;

	hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
	hw_render.version_major = 3;
	hw_render.version_minor = 3;
	hw_render.context_reset = context_reset;
	hw_render.context_destroy = context_destroy;
	hw_render.bottom_left_origin = true;
	hw_render.depth = true;
	//	hw_render.cache_context = true;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
		printf("Failed to create RETRO_HW_CONTEXT_OPENGL_CORE;\n");

	Input::Init();

	return true;
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
	GetCoreThread().Suspend(true);
	//			GetCoreThread().Cancel(true);
}


void retro_run(void)
{
	Options::CheckVariables();
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
int numSpeakers;
bool EffectsDisabled = false;
float FinalVolume = 1.0f; // Global / pre-scale
bool AdvancedVolumeControl;
float VolumeAdjustFLdb;
float VolumeAdjustCdb;
float VolumeAdjustFRdb;
float VolumeAdjustBLdb;
float VolumeAdjustBRdb;
float VolumeAdjustSLdb;
float VolumeAdjustSRdb;
float VolumeAdjustLFEdb;
bool postprocess_filter_enabled = 1;
bool postprocess_filter_dealias = false;
int dplLevel;
u32 OutputModule;
int SndOutLatencyMS;
int SynchMode;

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
	static s16 snd_buffer[0x200 << 1];
	snd_buffer[write_pos++] = Sample.Left >> 12;
	snd_buffer[write_pos++] = Sample.Right >> 12;
	if(write_pos == (0x200 << 1))
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

void SndBuffer::UpdateTempoChangeAsyncMixing()
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

void SysMessage(const char* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	vprintf(fmt, list);
	va_end(list);
}

wxEventLoopBase* Pcsx2AppTraits::CreateEventLoop()
{
	return new wxEventLoop();
	// return new wxGUIEventLoop();
	// return new wxConsoleEventLoop();
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
