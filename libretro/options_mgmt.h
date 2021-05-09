#include <libretro.h>
#include <stdlib.h>

static constexpr const char* BOOL_PCSX2_OPT_FASTBOOT = "pcsx2_fastboot";
static constexpr const char* BOOL_PCSX2_OPT_FORCE_WIDESCREEN = "pcsx2_force_widescreen";
static constexpr const char* BOOL_PCSX2_OPT_ENABLE_PRESETS = "pcsx2_enable_presets";
static constexpr const char* BOOL_PCSX2_OPT_FRAMESKIP = "pcsx2_frameskip";

static constexpr const char* STRING_PCSX2_OPT_BIOS = "pcsx2_bios";
static constexpr const char* STRING_PCSX2_OPT_RENDERER = "pcsx2_renderer";

static constexpr const char* INT_PCSX2_OPT_UPSCALE_MULTIPLIER = "pcsx2_upscale_multiplier";
static constexpr const char* INT_PCSX2_OPT_PRESET_INDEX = "pcsx2_preset_index";
static constexpr const char* INT_PCSX2_OPT_FRAMES_TO_DRAW= "pcsx2_frames_to_draw";
static constexpr const char* INT_PCSX2_OPT_FRAMES_TO_SKIP = "pcsx2_frames_to_skip";
static constexpr const char* INT_PCSX2_OPT_RENDERER_THREADS = "pcsx2_sw_renderer_threads";


enum class KeyOptionBool
{
	bool_return,
};

enum class KeyOptionInt
{
	int_return,
};

enum class KeyOptionString
{
	string_return,

};


static bool option_value(retro_environment_t environ_cb, const char* const_option, KeyOptionBool return_type) {
	struct retro_variable var = { 0 };
	var.key = const_option;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
	{
		if (strcmp(var.value, "enabled") == 0)
			return true;
	}
	return false;
}


static int option_value(retro_environment_t environ_cb, const char* const_option, KeyOptionInt return_type) {
	struct retro_variable var = { 0 };
		var.key = const_option;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
	{
		return atoi(var.value);
	}
	return 0;
}


static const char* option_value(retro_environment_t environ_cb, const char* const_option, KeyOptionString return_type) {
	struct retro_variable var = { 0 };
		var.key = const_option;
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
		{
			return var.value;
		}
	return NULL;
}


