#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__
#include <stdlib.h>
#include <string.h>
#include <libretro.h>
#include <retro_inline.h>
#endif


struct retro_core_option_definition option_defs[] = {

	{"pcsx2_bios",
	"System: BIOS",
	NULL,
	{
		// dynamically filled in retro_init
	},
	NULL},

	{"pcsx2_system_language",
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

	{"pcsx2_fastboot",
	"System: Fast Boot",
	"Bypass the intial BIOS logo. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{"pcsx2_boot_bios",
	"System: Boot To BIOS",
	"Skip the content loading and boot in the BIOS. Useful to manage memory cards currently associated to this content. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{"pcsx2_memcard_slot_1",
	"Memory Card: Slot 1",
	"Select the primary memory card to use. 'Legacy' points to the memory card Mcd001 in the old location system/pcsx2/memcards. (Content restart required)",
	{
		// dynamically filled in retro_init
	},
	NULL},

	{ "pcsx2_memcard_slot_2",
	"Memory Card: Slot 2",
	"Select the secondary memory card to use. 'Legacy' points to the memory card Mcd002 in the old location system/pcsx2/memcards. (Content restart required)",
	{
		// dynamically filled in retro_init
	},
	NULL },


	{"pcsx2_renderer",
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

	{"pcsx2_upscale_multiplier",
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

	{"pcsx2_deinterlace_mode",
	"Video: Deinterlacing Mode",
	"Can remove blur on some games. Note that some modes can slightly decrease performance while the others can blur the whole picture, decreasing the amount of details thus deinterlacing is to be used only when it's necessary.",
	{
		{"7", "Automatic (default)"},
		{"6", "Blend bff - slight blur, 1/2 fps"},
		{"5", "Blend tff - slight blur, 1/2 fps"},
		{"4", "Bob bff   - use blend if shaking"},
		{"3", "Bob tff   - use blend if shaking"},
		{"2", "Weave bff - saw-tooth"},
		{"1", "Weave tff - saw-tooth"},
		{"0", "None "},
		{NULL, NULL},
	},
	"7"},

	{"pcsx2_aspect_ratio",
	"Video: Aspect Ratio",
	"Sets the aspect ratio. Setting the aspect ratio to Widescreen (16:9) stretches the display. For proper widescreen in select games, also turn on the 'Enable Widescreen Patches' option. (Content restart required)",
	{
		{"0", "Standard (4:3)"},
		{"1", "Widescreen (16:9)"},
		{NULL, NULL},
	},
	"0"},

	{"pcsx2_enable_widescreen_patches",
	"Video: Enable Widescreen Patches",
	"Enables widescreen patches that allow certain games to render in true 16:9 ratio without stretching the display. For the widescreen patches to display properly, the 'Aspect Ratio' option should be set to Widescreen (16:9). (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{ "pcsx2_fxaa",
	"Video: FXAA",
	"Enables Fast Approximate Anti-Aliasing",
	{
		{"0", "Disabled"},
		{"1", "Enabled"},
		{NULL, NULL},
	},
	"0" },

	{"pcsx2_anisotropic_filter",
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

	{ "pcsx2_texture_filtering",
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

	{ "pcsx2_frameskip",
	"Video: Frame Skip",
	NULL,
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{ "pcsx2_frames_to_draw",
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

	{ "pcsx2_frames_to_skip",
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

	{"pcsx2_rumble_enable",
	"Gamepad: Enable Rumble",
	"Enables rumble on gamepads that support it",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"enabled"},

	{"pcsx2_rumble_intensity",
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

	{ "pcsx2_enable_cheats",
	"Patches: Enable Cheats",
	"Enabled: Checks the 'system/pcsx2/cheats' directory for a PNACH file for the running content and, if found, \
	applies the cheats from the file. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{"pcsx2_speedhacks_presets",
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

	{ "pcsx2_vsync_mtgs_queue",
	"Emulation: Vsyncs in MTGS Queue",
	"Setting this to a lower value improves input lag, a value around 2 or 3 will sightly improve framerates. (Content restart required)",
	{
		{"0", "0"},
		{"1", "1"},
		{"2", "2 (default)"},
		{"3", "3"},
		{NULL, NULL},
	},
	"2" },


	{ "pcsx2_userhack_align_sprite",
	"Hack: Align Sprite",
	"Fixes vertical lines problem in some games when resolution is upscaled.",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{ "pcsx2_userhack_merge_sprite",
	"Hack: Merge Sprite",
	"Another option which could fix vertical lines problem in some games when resolution is upscaled.",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },


	{ "pcsx2_userhack_skipdraw_start",
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

	{ "pcsx2_userhack_skipdraw_layers",
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

	{ "pcsx2_userhack_halfpixel_offset",
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

	{ "pcsx2_userhack_round_sprite",
	"Hack: Round Sprite",
	"Corrects the sampling of 2D textures when upscaling.",
	{
		{"0", "Off (default)"},
		{"1", "Half"},
		{"2", "Full"},
		{NULL, NULL},
	},
	"0" },

	{ "pcsx2_userhack_wildarms_offset",
	"Hack: Wild Arms Offset",
	"Avoid gaps between pixel in some games when upscaling.",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{ "pcsx2_userhack_halfscreen_fix",
	"Hack: Half-screen fix",
	"Automatic control of the halfscreen fix detection on texture shuffle. Force-Disable may help in some games, but causes visual glitches in most. Use Force-Enable when a game has half screen issues.",
	{
		{"-1", "Automatic (default)"},
		{"0", "Force-Disabled"},
		{"1", "Force-Enabled"},
		{NULL, NULL},
	},
	"-1" },

	{ "pcsx2_userhack_auto_flush",
	"Hack: Auto Flush",
	"Force a primitive flush when a framebuffer is also an input texture. Fixes some processing effects such as the shadows in the \
	Jak series and radiosity in GTA:SA. Very costly in performance. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{ "pcsx2_userhack_texture_offset_x_hundreds",
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

	{ "pcsx2_userhack_texture_offset_x_tens",
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

	{ "pcsx2_userhack_texture_offset_y_hundreds",
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

	{ "pcsx2_userhack_texture_offset_y_tens",
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
