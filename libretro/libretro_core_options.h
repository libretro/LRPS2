#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__
#include <stdlib.h>
#include <string.h>
#include <libretro.h>
#include <retro_inline.h>
#include "options_enums.h"
#endif

struct retro_core_option_definition option_defs[] = {

	{STRING_PCSX2_OPT_BIOS,
	"System: BIOS",
	NULL,
	{
		// dynamically filled in retro_init
	},
	NULL},

	{STRING_PCSX2_OPT_SYSTEM_LANGUAGE,
	"System: Language",
	"Set the BIOS system Language. Useful for PAL multilanguage games. Fastboot option must be disabled. The selected language will be applied, if available in-game. (Content restart required)",
	{
		{"English", NULL},
		{"French", NULL},
		{"Spanish", NULL},
		{"German", NULL},
		{"Italian", NULL},
		{"Dutch", NULL},
		{"Portuguese", NULL},
		{NULL, NULL},
	},
	"English"},

	{BOOL_PCSX2_OPT_FASTCDVD,
	"System: Fast Loading",
	"A hack that reduces loading times by setting a faster disc access mode. Check the HDLoader compatibility list for games that will not work with this (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{BOOL_PCSX2_OPT_FASTBOOT,
	"System: Fast Boot",
	"Bypass the initial BIOS logo. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{BOOL_PCSX2_OPT_BOOT_TO_BIOS,
	"System: Boot To BIOS",
	"Skip the content loading and boot in the BIOS. Useful to manage memory cards currently associated to this content. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{STRING_PCSX2_OPT_MEMCARD_SLOT_1,
	"Memory Card: Slot 1",
	"Select the primary memory card to use. 'Legacy' points to the memory card Mcd001 in the old location system/pcsx2/memcards. (Content restart required)",
	{
		// dynamically filled in retro_init
	},
	NULL},

	{STRING_PCSX2_OPT_MEMCARD_SLOT_2,
	"Memory Card: Slot 2",
	"Select the secondary memory card to use. 'Legacy' points to the memory card Mcd002 in the old location system/pcsx2/memcards. (Content restart required)",
	{
		// dynamically filled in retro_init
	},
	NULL },


	{STRING_PCSX2_OPT_RENDERER,
	"Video: Renderer",
	"Content reboot required",
	{
		{"Auto", NULL},
#ifdef _WIN32
		{"D3D11", NULL},
#endif
		{"OpenGl", NULL},
		{NULL, NULL},
	},
	"Auto"},

	{INT_PCSX2_OPT_UPSCALE_MULTIPLIER,
	"Video: Internal Resolution ",
	NULL,
	{
		{"1", "Native PS2"},
		{"2", "2x Native ~720p"},
		{"3", "3x Native ~1080p"},
		{"4", "4x Native ~1440p 2K"},
		{"5", "5x Native ~1620p 3K"},
		{"6", "6x Native ~2160p 4K"},
		{"8", "8x Native ~2880p 5K"},
		{NULL, NULL},
	},
	"1"},

	{INT_PCSX2_OPT_DEINTERLACING_MODE,
	"Video: Deinterlacing Mode",
	"Can remove blur on some games. Some modes can slightly decrease performance while others can blur the whole picture, decreasing the amount of details thus deinterlacing is to be used only when it's necessary. NOTE: Setting this to 'No interlacing patch' will try to apply a no-interlacing patch if available in the database. No-interlacing has the best image quality, but might not always be available for your game. NOTE: No-interlacing option requires a content restart.",
	{
		{"7", "Automatic (default)"},
		{"6", "Blend bff - slight blur, 1/2 fps"},
		{"5", "Blend tff - slight blur, 1/2 fps"},
		{"4", "Bob bff   - use blend if shaking"},
		{"3", "Bob tff   - use blend if shaking"},
		{"2", "Weave bff - saw-tooth"},
		{"1", "Weave tff - saw-tooth"},
		{"0", "None "},
		{"-1","No-interlacing patch"},
		{NULL, NULL},
	},
	"7"},

	{INT_PCSX2_OPT_ASPECT_RATIO,
	"Video: Aspect Ratio",
	"Sets the aspect ratio. Setting the aspect ratio to Widescreen (16:9) stretches the display. For proper widescreen in select games, also turn on the 'Enable Widescreen Patches' option. (Content restart required)",
	{
		{"0", "Standard (4:3)"},
		{"1", "Widescreen (16:9)"},
		{NULL, NULL},
	},
	"0"},

	{BOOL_PCSX2_OPT_ENABLE_WIDESCREEN_PATCHES,
	"Video: Enable Widescreen Patches",
	"Enables widescreen patches that allow certain games to render in true 16:9 ratio without stretching the display. For the widescreen patches to display properly, the 'Aspect Ratio' option should be set to Widescreen (16:9). (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{BOOL_PCSX2_OPT_ENABLE_60FPS_PATCHES,
	"Video: Enable 60fps Patches",
	"Enables 60fps patches that allow certain games to run at higher framerates than the original framerate. NOTE: No guarantees about stability or game bugs, use at your own caution. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{INT_PCSX2_OPT_FXAA,
	"Video: FXAA",
	"Enables Fast Approximate Anti-Aliasing",
	{
		{"0", "Disabled"},
		{"1", "Enabled"},
		{NULL, NULL},
	},
	"0" },

	{INT_PCSX2_OPT_ANISOTROPIC_FILTER,
	 "Video: Anisotropic Filtering",
	 "Reduces texture aliasing at extreme viewing angles",
	{
		{"0", "None"},
		{"2", "2x"},
		{"4", "4x"},
		{"8", "8x"},
		{"16", "16x"},
		{NULL, NULL},
	},
	"0" },

	{ INT_PCSX2_OPT_DITHERING,
	 "Video: Dithering",
	 "Disabling dithering can remove some noise and pixelate effects",
	{
		{"0", "Off"},
		{"1", "Scaled"},
		{"2", "Unscaled (default)"},
		{NULL, NULL},
	},
	"2" },

	{INT_PCSX2_OPT_TEXTURE_FILTERING,
	"Video: Texture Filtering",
	"Controls the texture filtering of the emulation "
		"\nNearest: always disable interpolation, rendering will be blocky. "
		"\nBilinear Forced: always enable interpolation. Rendering is smoother but it could generate some glitches. "
		"\nBilinear PS2: use same mode as the PS2. It is the more accurate option."
		"\nBilinear Forced (excluding sprite): always enable interpolation except for sprites (FMV/Text/2D elements). Rendering is smoother but it could generate a few glitches. If upscaling is enabled, this setting is recommended over 'Bilinear Forced ",
	{
		{"0", "Nearest"},
		{"1", "Bilinear (Forced)"},
		{"2", "Bilinear (PS2 - Default)"},
		{"3", "Bilinear (Forced Excluding Sprite)"},
		{NULL, NULL},
	},
	"2" },

	{INT_PCSX2_OPT_MIPMAPPING,
	 "Video: Mipmapping",
	 "Control the accuracy level of the mipmapping emulation."
		"\nAutomatic: Automatically sets the mipmapping level based on the game.This is the recommended setting."
		"\nOff: Mipmapping emulation is disabled."
		"\nBasic (Fast): Partially emulates mipmapping, performance impact is negligible in most cases."
		"\nFull (Slow):  Completely emulates the mipmapping function of the GS, might significantly impact performance.",
	{
		{"-1", "Automatic (default)"},
		{"0", "Off"},
		{"1", "Basic"},
		{"2", "Full"},

		{NULL, NULL},
	},
	"-1" },

	{BOOL_PCSX2_OPT_CONSERVATIVE_BUFFER,
	"Video: Conservative Buffer Allocation",
	"Disabled: Reserves a larger framebuffer to prevent FMV flickers. Increases GPU/memory requirements."
		"\n\nDisabling this can amplify stuttering due to low RAM/VRAM."
		"\nNote: It should be enabled for Armored Core, Destroy All Humans, Gran Turismo and possibly others."
		"\nThis option does not improve the graphics or the FPS.",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"enabled" },

	{BOOL_PCSX2_OPT_ACCURATE_DATE,
	"Video: Accurate DATE",
	"Implement a more accurate algorithm to compute GS destination alpha testing. It improves shadow and transparency rendering."
				"\nNote: Direct3D 11 is less accurate.",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"enabled" },


	{BOOL_PCSX2_OPT_FRAMESKIP,
	"Video: Frame Skip",
	NULL,
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{INT_PCSX2_OPT_FRAMES_TO_DRAW,
	"Video: Frameskip - Frames to Draw",
	NULL,
	{
		{"1", NULL},
		{"2", NULL},
		{"3", NULL},
		{"4", NULL},
		{"5", NULL},
		{"6", NULL},
		{"7", NULL},
		{"8", NULL},
		{"9", NULL},
		{"10", NULL},
		{NULL, NULL},
	},
	"1" },

	{INT_PCSX2_OPT_FRAMES_TO_SKIP,
	"Video: Frameskip - Frames to Skip",
	NULL,
	{
		{"1", NULL},
		{"2", NULL},
		{"3", NULL},
		{"4", NULL},
		{"5", NULL},
		{"6", NULL},
		{"7", NULL},
		{"8", NULL},
		{"9", NULL},
		{"10", NULL},
		{NULL, NULL},
	},
	"1" },

	{BOOL_PCSX2_OPT_GAMEPAD_RUMBLE_ENABLE,
	"Gamepad: Enable Rumble",
	"Enables rumble on gamepads that support it",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"enabled"},

	{INT_PCSX2_OPT_GAMEPAD_RUMBLE_FORCE,
	"Gamepad: Rumble Intensity",
	"Intensity of gamepad rumble",
	{
		{"10", "10%"},
		{"20", "20%"},
		{"30", "30%"},
		{"40", "40%"},
		{"50", "50%"},
		{"60", "60%"},
		{"70", "70%"},
		{"80", "80%"},
		{"90", "90%"},
		{"100", "100%"},
		{NULL, NULL},
	},
	"100"},

	{ INT_PCSX2_OPT_GAMEPAD_L_DEADZONE,
	"Gamepad: Left Thumbstick Deadzone",
	"Set the dead zone of left thumbstick",
	{
		{"0", "No Dead Zone (default)"},
		{"1", "1%"},
		{"2", "2%"},
		{"3", "3%"},
		{"4", "4%"},
		{"5", "5%"},
		{"6", "6%"},
		{"7", "7%"},
		{"8", "8%"},
		{"9", "9%"},
		{"10", "10%"},
		{"11", "11%"},
		{"12", "12%"},
		{"13", "13%"},
		{"14", "14%"},
		{"15", "15%"},
		{"16", "16%"},
		{"17", "17%"},
		{"18", "18%"},
		{"19", "19%"},
		{"20", "20%"},
		{"21", "21%"},
		{"22", "22%"},
		{"23", "23%"},
		{"24", "24%"},
		{"25", "25%"},
		{"26", "26%"},
		{"27", "27%"},
		{"28", "28%"},
		{"29", "29%"},
		{"30", "30%"},
		{"31", "31%"},
		{"32", "32%"},
		{"33", "33%"},
		{"34", "34%"},
		{"35", "35%"},
		{"36", "36%"},
		{"37", "37%"},
		{"38", "38%"},
		{"39", "39%"},
		{"40", "40%"},
		{"41", "41%"},
		{"42", "42%"},
		{"43", "43%"},
		{"44", "44%"},
		{"45", "45%"},
		{"46", "46%"},
		{"47", "47%"},
		{"48", "48%"},
		{"49", "49%"},
		{"50", "50%"},
		{NULL, NULL},
	},
	"enabled" },


	{ INT_PCSX2_OPT_GAMEPAD_R_DEADZONE,
	"Gamepad: Right Thumbstick Deadzone",
	"Set the dead zone of right thumbstick",
	{
		{"0", "No Dead Zone (default)"},
		{"1", "1%"},
		{"2", "2%"},
		{"3", "3%"},
		{"4", "4%"},
		{"5", "5%"},
		{"6", "6%"},
		{"7", "7%"},
		{"8", "8%"},
		{"9", "9%"},
		{"10", "10%"},
		{"11", "11%"},
		{"12", "12%"},
		{"13", "13%"},
		{"14", "14%"},
		{"15", "15%"},
		{"16", "16%"},
		{"17", "17%"},
		{"18", "18%"},
		{"19", "19%"},
		{"20", "20%"},
		{"21", "21%"},
		{"22", "22%"},
		{"23", "23%"},
		{"24", "24%"},
		{"25", "25%"},
		{"26", "26%"},
		{"27", "27%"},
		{"28", "28%"},
		{"29", "29%"},
		{"30", "30%"},
		{"31", "31%"},
		{"32", "32%"},
		{"33", "33%"},
		{"34", "34%"},
		{"35", "35%"},
		{"36", "36%"},
		{"37", "37%"},
		{"38", "38%"},
		{"39", "39%"},
		{"40", "40%"},
		{"41", "41%"},
		{"42", "42%"},
		{"43", "43%"},
		{"44", "44%"},
		{"45", "45%"},
		{"46", "46%"},
		{"47", "47%"},
		{"48", "48%"},
		{"49", "49%"},
		{"50", "50%"},
		{NULL, NULL},
	},
	"enabled" },

	{BOOL_PCSX2_OPT_ENABLE_CHEATS,
	"Patches: Enable Cheats",
	"Enabled: Checks the 'system/pcsx2/cheats' directory for a PNACH file for the running content and, if found, \
	applies the cheats from the file. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{INT_PCSX2_OPT_SPEEDHACKS_PRESET,
	 "Emulation: Speed Hacks Preset",
	 "Preset which controls the balance between accuracy and speed. (Content restart required) \
		\nSafest: no speed hacks. Most reliable, but possibly slow. \
		\nSafe: a few speed hacks known to provide boosts with minimal to no side effects. \
		\nBalanced: recommended preset for CPUs with 4+ cores. Provides good boosts with minimal to no side effects. \
		\nAggressive/Very Aggressive: may help underpowered CPUs in less demanding games, but may cause problems in other cases. \
		\nMostly Harmful: helps a very small set of games with unusual performance requirements. Not recommended for underpowered devices.",
	{
		{"0", "Safest - No Hacks"},
		{"1", "Safe (default)"},
		{"2", "Balanced"},
		{"3", "Aggressive"},
		{"4", "Very Aggressive"},
		{"5", "Mostly Harmful"},
		{NULL, NULL},
	},
	"1" },

	{INT_PCSX2_OPT_VSYNC_MTGS_QUEUE,
	"Emulation: Vsyncs in MTGS Queue",
	"Setting this to a lower value improves input lag, a value around 2 or 3 will slightly improve framerates.",
	{
		{"0", "0"},
		{"1", "1"},
		{"2", "2 (default)"},
		{"3", "3"},
		{NULL, NULL},
	},
	"2" },

	{INT_PCSX2_OPT_EE_CLAMPING_MODE,
	"Emulation: EE/FPU Clamping Mode",
	"EE/FPU clamping mode can fix some bugs on some games. Default value is fine for most games. (Content restart required)",
	{
		{"0", "None"},
		{"1", "Normal (default)"},
		{"2", "Extra + Preserve Sign"},
		{"3", "Full"},
		{NULL, NULL},
	},
	"1" },


	{INT_PCSX2_OPT_EE_ROUND_MODE,
	"Emulation: EE/FPU Round Mode",
	"EE/FPU round mode can fix some bugs on some games. Default value is fine for most games. (Content restart required)",
	{
		{"0", "Nearest"},
		{"1", "Negative"},
		{"2", "Positive"},
		{"3", "Chop/Zero (default)"},
		{NULL, NULL},
	},
	"3" },


	{ INT_PCSX2_OPT_VU_CLAMPING_MODE,
	"Emulation: VUs Clamping Mode",
	"VUs clamping mode can fix some bugs on some games. Default value is fine for most games. (Content restart required)",
	{
		{"0", "None"},
		{"1", "Normal (default)"},
		{"2", "Extra"},
		{"3", "Extra + Preserve Sign"},
		{NULL, NULL},
	},
	"1" },


	{ INT_PCSX2_OPT_VU_ROUND_MODE,
	"Emulation: VUs Round Mode",
	"VUs round mode can fix some bugs on some games. Default value is fine for most games. (Content restart required)",
	{
		{"0", "Nearest"},
		{"1", "Negative"},
		{"2", "Positive"},
		{"3", "Chop/Zero (default)"},
		{NULL, NULL},
	},
	"3" },


	{BOOL_PCSX2_OPT_USERHACK_ALIGN_SPRITE,
	"Hack: Align Sprite",
	"Fixes vertical lines problem in some games when resolution is upscaled.",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{BOOL_PCSX2_OPT_USERHACK_MERGE_SPRITE,
	"Hack: Merge Sprite",
	"Another option which could fix vertical lines problem in some games when resolution is upscaled.",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },


	{INT_PCSX2_OPT_USERHACK_SKIPDRAW_START,
	"Hack: Skipdraw - Start Layer",
	"Used to fix some rendering glitches and bad post processing by skipping rendering layers.",
	{
		{"0", "0 (default)"},
		{"1", NULL},
		{"2", NULL},
		{"3", NULL},
		{"4", NULL},
		{"5", NULL},
		{"6", NULL},
		{"7", NULL},
		{"8", NULL},
		{"9", NULL},
		{"10", NULL},
		{"11", NULL},
		{"12", NULL},
		{"13", NULL},
		{"14", NULL},
		{"15", NULL},
		{"16", NULL},
		{"17", NULL},
		{"18", NULL},
		{"19", NULL},
		{"20", NULL},
		{NULL, NULL},
	},
	"0" },

	{INT_PCSX2_OPT_USERHACK_SKIPDRAW_LAYERS,
	"Hack: Skipdraw - Layers to Skip",
	"Number of rendering layers to skip, counting from the value set in the Start Layer option. For a original PCSX2 setting of 2:5, skipdraw option of the core must be set 2 +3 .",
	{
		{"0", "+0 (default)"},
		{"1", "+1"},
		{"2", "+2"},
		{"3", "+3"},
		{"4", "+4"},
		{"5", "+5"},
		{"6", "+6"},
		{"7", "+7"},
		{"8", "+8"},
		{"9", "+9"},
		{"10", "+10"},
		{"11", "+11"},
		{"12", "+12"},
		{"13", "+13"},
		{"14", "+14"},
		{"15", "+15"},
		{"16", "+16"},
		{"17", "+17"},
		{"18", "+18"},
		{"19", "+19"},
		{"20", "+20"},
		{NULL, NULL},
	},
	"0" },

	{INT_PCSX2_OPT_USERHACK_HALFPIXEL_OFFSET,
	"Hack: Half-pixel Offset",
	"Might fix some misaligned fog, bloom or blend effect. The preferred option is Normal(vertex).",
	{
		{"0", "Off (default)"},
		{"1", "Normal (Vertex)"},
		{"2", "Special (Texture)"},
		{"3", "Special (Texture-Aggessive)"},
		{NULL, NULL},
	},
	"0" },

	{INT_PCSX2_OPT_USERHACK_ROUND_SPRITE,
	"Hack: Round Sprite",
	"Corrects the sampling of 2D textures when upscaling.",
	{
		{"0", "Off (default)"},
		{"1", "Half"},
		{"2", "Full"},
		{NULL, NULL},
	},
	"0" },

	{BOOL_PCSX2_OPT_USERHACK_WILDARMS_OFFSET,
	"Hack: Wild Arms Offset",
	"Avoid gaps between pixel in some games when upscaling.",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{INT_PCSX2_OPT_USERHACK_HALFSCREEN_FIX,
	"Hack: Half-screen fix",
	"Automatic control of the halfscreen fix detection on texture shuffle. Force-Disable may help in some games, but causes visual glitches in most. Use Force-Enable when a game has half screen issues.",
	{
		{"-1", "Automatic (default)"},
		{"0", "Force-Disabled"},
		{"1", "Force-Enabled"},
		{NULL, NULL},
	},
	"-1" },

	{BOOL_PCSX2_OPT_USERHACK_AUTO_FLUSH,
	"Hack: Auto Flush",
	"Force a primitive flush when a framebuffer is also an input texture. Fixes some processing effects such as the shadows in the \
	Jak series and radiosity in GTA:SA. Very costly in performance. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_X_HUNDREDS,
	"Hack: Texture Offset X - Hundreds",
	"Set Texture Offset X (sum of X options Hundreds and Tens). \n Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment too.",
	{
		{"0", "0 (default)"},
		{"100", "100"},
		{"200", "200"},
		{"300", "300"},
		{"400", "400"},
		{"500", "500"},
		{"600", "600"},
		{"700", "700"},
		{"800", "800"},
		{"900", "900"},
		{NULL, NULL},
	},
	"0" },

	{INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_X_TENS,
	"Hack: Texture Offset X - Tens",
	"Set Texture Offset X (sum of X options Hundreds and Tens). \n Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment too.",
	{
		{"0", "0 (default)"},
		{"10", "10"},
		{"20", "20"},
		{"30", "30"},
		{"40", "40"},
		{"50", "50"},
		{"60", "60"},
		{"70", "70"},
		{"80", "80"},
		{"90", "90"},
		{NULL, NULL},
	},
	"0" },

	{INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_Y_HUNDREDS,
	"Hack: Texture Offset Y - Hundreds",
	"Set Texture Offset Y (sum of Y options Hundreds and Tens). \n Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment too.",
	{
		{"0", "0 (default)"},
		{"100", "100"},
		{"200", "200"},
		{"300", "300"},
		{"400", "400"},
		{"500", "500"},
		{"600", "600"},
		{"700", "700"},
		{"800", "800"},
		{"900", "900"},
		{NULL, NULL},
	},
	"0" },

	{INT_PCSX2_OPT_USERHACK_TEXTURE_OFFSET_Y_TENS,
	"Hack: Texture Offset Y - Tens",
	"Set Texture Offset Y (sum of Y options Hundreds and Tens). \n Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment too.",
	{
		{"0", "0 (default)"},
		{"10", "10"},
		{"20", "20"},
		{"30", "30"},
		{"40", "40"},
		{"50", "50"},
		{"60", "60"},
		{"70", "70"},
		{"80", "80"},
		{"90", "90"},
		{NULL, NULL},
	},
	"0" },

	{NULL, NULL, NULL, {{0}}, NULL},
};

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
	unsigned version = 0;

	if (!environ_cb)
		return;

	if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version >= 1))
	{
		environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, &option_defs);
	}
	else
	{
		size_t i;
		size_t num_options = 0;
		struct retro_variable* variables = NULL;
		char** values_buf = NULL;

		/* Determine number of options */
		while (true)
		{
			if (option_defs[num_options].key)
				num_options++;
			else
				break;
		}

		/* Allocate arrays */
		variables = (struct retro_variable*)calloc(num_options + 1, sizeof(struct retro_variable));
		values_buf = (char**)calloc(num_options, sizeof(char*));

		if (!variables || !values_buf)
			goto error;

		/* Copy parameters from option_defs array */
		for (i = 0; i < num_options; i++)
		{
			const char* key = option_defs[i].key;
			const char* desc = option_defs[i].desc;
			const char* default_value = option_defs[i].default_value;
			struct retro_core_option_value* values = option_defs[i].values;
			size_t buf_len = 3;
			size_t default_index = 0;

			values_buf[i] = NULL;

			if (desc)
			{
				size_t num_values = 0;

				/* Determine number of values */
				while (true)
				{
					if (values[num_values].value)
					{
						/* Check if this is the default value */
						if (default_value)
							if (strcmp(values[num_values].value, default_value) == 0)
								default_index = num_values;

						buf_len += strlen(values[num_values].value);
						num_values++;
					}
					else
						break;
				}

				/* Build values string */
				if (num_values > 0)
				{
					size_t j;

					buf_len += num_values - 1;
					buf_len += strlen(desc);

					values_buf[i] = (char*)calloc(buf_len, sizeof(char));
					if (!values_buf[i])
						goto error;

					strcpy(values_buf[i], desc);
					strcat(values_buf[i], "; ");

					/* Default value goes first */
					strcat(values_buf[i], values[default_index].value);

					/* Add remaining values */
					for (j = 0; j < num_values; j++)
					{
						if (j != default_index)
						{
							strcat(values_buf[i], "|");
							strcat(values_buf[i], values[j].value);
						}
					}
				}
			}

			variables[i].key = key;
			variables[i].value = values_buf[i];
		}

		/* Set variables */
		environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

	error:

		/* Clean up */
		if (values_buf)
		{
			for (i = 0; i < num_options; i++)
			{
				if (values_buf[i])
				{
					free(values_buf[i]);
					values_buf[i] = NULL;
				}
			}

			free(values_buf);
			values_buf = NULL;
		}

		if (variables)
		{
			free(variables);
			variables = NULL;
		}
	}
}
