/*
 * Options tools
 * The aim is to provide the access to core options through the call of the same function
 * indipendently from the return types, improving readability and maintaining of the code.
 * The function is exposed with overloads which return boolean, string or int.
 * The KeyOption enum types are used only as a trick to select the needed overloaded function.
 * 
 */

#include <libretro.h>
#include <stdlib.h>
#include <cstring>

extern retro_environment_t environ_cb;
extern retro_log_printf_t log_cb;
extern void GSUpdateOptions();
extern void ResetContentStuffs();
extern int option_upscale_mult;


static const char* BOOL_PCSX2_OPT_FASTBOOT					= "pcsx2_fastboot";
static const char* BOOL_PCSX2_OPT_ENABLE_WIDESCREEN_PATCHES = "pcsx2_enable_widescreen_patches";
static const char* BOOL_PCSX2_OPT_FRAMESKIP					= "pcsx2_frameskip";
static const char* BOOL_PCSX2_OPT_USERHACK_ALIGN_SPRITE		= "pcsx2_userhack_align_sprite";
static const char* BOOL_PCSX2_OPT_USERHACK_MERGE_SPRITE		= "pcsx2_userhack_merge_sprite";
static const char* BOOL_PCSX2_OPT_USERHACK_WILDARMS_OFFSET	= "pcsx2_userhack_wildarms_offset";
static const char* BOOL_PCSX2_OPT_GAMEPAD_RUMBLE_ENABLE		= "pcsx2_rumble_enable";
static const char* BOOL_PCSX2_OPT_BOOT_TO_BIOS				= "pcsx2_boot_bios";
static const char* BOOL_PCSX2_OPT_ENABLE_CHEATS				= "pcsx2_enable_cheats";
static const char* BOOL_PCSX2_OPT_USERHACK_AUTO_FLUSH		= "pcsx2_userhack_auto_flush";
static const char* BOOL_PCSX2_OPT_CONSERVATIVE_BUFFER		= "pcsx2_conservative_buffer";
static const char* BOOL_PCSX2_OPT_ACCURATE_DATE			    = "pcsx2_accurate_date";




static const char* STRING_PCSX2_OPT_BIOS					= "pcsx2_bios";
static const char* STRING_PCSX2_OPT_RENDERER				= "pcsx2_renderer";
static const char* STRING_PCSX2_OPT_SYSTEM_LANGUAGE			= "pcsx2_system_language";
static const char* STRING_PCSX2_OPT_MEMCARD_SLOT_1			= "pcsx2_memcard_slot_1";
static const char* STRING_PCSX2_OPT_MEMCARD_SLOT_2			= "pcsx2_memcard_slot_2";


static const char* INT_PCSX2_OPT_ASPECT_RATIO				= "pcsx2_aspect_ratio";
static const char* INT_PCSX2_OPT_UPSCALE_MULTIPLIER			= "pcsx2_upscale_multiplier";
static const char* INT_PCSX2_OPT_SPEEDHACKS_PRESET			= "pcsx2_speedhacks_presets";
static const char* INT_PCSX2_OPT_FRAMES_TO_DRAW				= "pcsx2_frames_to_draw";
static const char* INT_PCSX2_OPT_FRAMES_TO_SKIP				= "pcsx2_frames_to_skip";
static const char* INT_PCSX2_OPT_ANISOTROPIC_FILTER			= "pcsx2_anisotropic_filter";
static const char* INT_PCSX2_OPT_USERHACK_SKIPDRAW_START	= "pcsx2_userhack_skipdraw_start";
static const char* INT_PCSX2_OPT_USERHACK_SKIPDRAW_LAYERS	= "pcsx2_userhack_skipdraw_layers";
static const char* INT_PCSX2_OPT_USERHACK_HALFPIXEL_OFFSET	= "pcsx2_userhack_halfpixel_offset";
static const char* INT_PCSX2_OPT_USERHACK_ROUND_SPRITE		= "pcsx2_userhack_round_sprite";
static const char* INT_PCSX2_OPT_USERHACK_HALFSCREEN_FIX	= "pcsx2_userhack_halfscreen_fix";
static const char* INT_PCSX2_OPT_GAMEPAD_RUMBLE_FORCE		= "pcsx2_rumble_intensity";
static const char* INT_PCSX2_OPT_DEINTERLACING_MODE			= "pcsx2_deinterlace_mode";
static const char* INT_PCSX2_OPT_FXAA						= "pcsx2_fxaa";
static const char* INT_PCSX2_OPT_TEXTURE_FILTERING			= "pcsx2_texture_filtering";
static const char* INT_PCSX2_OPT_VSYNC_MTGS_QUEUE			= "pcsx2_vsync_mtgs_queue";
static const char* INT_PCSX2_OPT_MIPMAPPING					= "pcsx2_mipmapping";
static const char* INT_PCSX2_OPT_CLAMPING_MODE				= "pcsx2_clamping_mode";
static const char* INT_PCSX2_OPT_ROUND_MODE					= "pcsx2_round_mode";



static const char* INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_X_HUNDREDS	= "pcsx2_userhack_texture_offset_x_hundreds";
static const char* INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_X_TENS		= "pcsx2_userhack_texture_offset_x_tens";
static const char* INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_Y_HUNDREDS = "pcsx2_userhack_texture_offset_y_hundreds";
static const char* INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_Y_TENS		= "pcsx2_userhack_texture_offset_y_tens";





enum class KeyOptionBool
{
	return_type,
};

enum class KeyOptionInt
{
	return_type,
};

enum class KeyOptionString
{
	return_type,

};


static bool option_value(const char* const_option, KeyOptionBool return_type) {
	struct retro_variable var = { 0 };
	var.key = const_option;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
	{
		if (strcmp(var.value, "enabled") == 0)
			return true;
	}
	else 
		log_cb(RETRO_LOG_ERROR, "Error while getting option: %s\n", var.key);
	
	return false;
}


static int option_value(const char* const_option, KeyOptionInt return_type) {
	struct retro_variable var = { 0 };
	var.key = const_option;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
		return atoi(var.value);
	log_cb(RETRO_LOG_ERROR, "Error while getting option: %s\n", var.key);
	return 0;
}


static const char* option_value(const char* const_option, KeyOptionString return_type) {
	struct retro_variable var = { 0 };
	var.key = const_option;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
		return var.value;
	log_cb(RETRO_LOG_ERROR, "Error while getting option: %s\n", var.key);
	return NULL;
}
