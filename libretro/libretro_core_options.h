#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__
#include <stdlib.h>
#include <string.h>
#include <libretro.h>
#include <retro_inline.h>
#endif


struct retro_core_option_definition option_defs[] = {

	{"pcsx2_bios",
	"Bios",
	NULL,
	{
		// dynamically filled in retro_init
	},
	NULL},

	{"pcsx2_system_language",
	"System Language",
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
	"Fast Boot",
	"Bypass the intial BIOS logo, with the side effect that BIOS settings like the system language will not be applied. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{"pcsx2_boot_bios",
	"Boot To BIOS",
	"Skip the content loading and boot in the BIOS. Useful to manage memory cards currently associated to this content. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},


	{"pcsx2_renderer",
	"Renderer",
	"Content reboot required",
	{
		{"Auto", NULL},
#ifdef _WIN32
		{"D3D11", NULL},
#endif
		{"OpenGl", NULL},
		{"Software", NULL},
		{"Null", NULL},
		{NULL, NULL},
	},
	"Auto"},

	{"pcsx2_upscale_multiplier",
	"Internal Resolution ",
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

	{"pcsx2_aspect_ratio",
	"Aspect Ratio",
	"Sets the aspect ratio. Setting the aspect ratio to Widescreen (16:9) stretches the display. For proper widescreen in select games, also turn on the 'Enable Widescreen Patches' option. (Content restart required)",
	{
		{"0", "Standard (4:3)"},
		{"1", "Widescreen (16:9)"},
		{NULL, NULL},
	},
	"0"},

	{"pcsx2_enable_widescreen_patches",
	"Enable Widescreen Patches",
	"Enables widescreen patches that allow certain games to render in true 16:9 ratio without stretching the display. For the widescreen patches to display properly, the 'Aspect Ratio' option should be set to Widescreen (16:9). Considerably increases games boot time. (Content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{"pcsx2_anisotropic_filter",
	 "Anisotropic Filtering",
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

	{"pcsx2_memcard_slot_1",
	"Memory Card: Slot 1",
	"Select the primary memory card to use. 'Legacy' points to the memory card Mcd001 in the old location system/pcsx2/memcards. (content restart required)",
	{
		// dynamically filled in retro_init
	},
	NULL},

	{ "pcsx2_memcard_slot_2",
	"Memory Card: Slot 2",
	"Select the secondary memory card to use. 'Legacy' points to the memory card Mcd002 in the old location system/pcsx2/memcards. (content restart required)",
	{
		// dynamically filled in retro_init
	},
	NULL },

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

	{"pcsx2_enable_speedhacks",
	"Enable SpeedHacks",
	"Speedhacks usually improve emulation speed, but can cause glitches, broken audio, and false FPS readings. When having emulation problems, or the BIOS menu text is not visible, disable this option. (content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{"pcsx2_speedhacks_presets",
	 "SpeedHacks preset",
	 "Preset which controls the speedhacks balance between accurancy and speed. This setting is applied only if 'Enable Speedhacks' option is enabled. (content restart required)",
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

	{"pcsx2_frameskip",
	"Frame Skip",
	NULL,
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{"pcsx2_frames_to_draw",
	"Frameskip: Frames to Draw",
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
	"1"},

	{ "pcsx2_frames_to_skip",
	"Frameskip: Frames to Skip",
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

	{ "pcsx2_sw_renderer_threads",
	"Software Rendered Threads",
	NULL,
	{
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
		{NULL, NULL},
	},
	"1" },

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
	"disabled" },

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
