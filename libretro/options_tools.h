#include <libretro.h>
#include <stdlib.h>
#include <cstring>
#include "options_enums.h"

extern retro_environment_t environ_cb;
extern retro_log_printf_t log_cb;
extern void GSUpdateOptions();
extern void ResetContentStuffs();
extern int option_upscale_mult;
extern int option_pad_left_deadzone;
extern int option_pad_right_deadzone;
extern bool option_palette_conversion;
extern bool hack_fb_conversion;
extern bool hack_AutoFlush;
extern bool hack_fast_invalidation;
/*
* These are quick fixes to provide system paths at pcsx2 app startup.
* Because of the huge refactoring, paths are not saved/loaded from inis files anymore,
* so happened that at startup of the app some things was broken because
* bios and other paths was provided later in the code flow
*/
extern std::string sel_bios_path;
extern std::string retroarch_system_path;


/*
 * Options tools
 * The aim is to provide the access to core options through the call of the same function
 * indipendently from the return types, improving readability and maintaining of the code.
 * The function is exposed with overloads which return boolean, string or int.
 * The KeyOption enum types are used only as a trick to select the needed overloaded function.
 * 
 */


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
