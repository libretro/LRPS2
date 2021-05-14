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

	{"pcsx2_fastboot",
	"Fast Boot",
	"This will bypass the intial bios logo, with the side effect that bios settings like the system language will not be applied. (Content restart required)",
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
	"auto"},

	{"pcsx2_upscale_multiplier",
	"Internal Resolution ",
	"Content restart required",
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

	{"pcsx2_force_widescreen",
	"Force Widescreen",
	"Content restart required",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{"pcsx2_enable_speedhacks",
	"Enable Speedhacks",
	"Speedhacks usually improve emulation speed, but can cause glitches, broken audio, and false FPS readings. When having emulation problems, or the BIOS menu text is not visible, disable this option.(content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled"},

	{"pcsx2_speedhacks_presets",
	 "Speedhacks preset",
	 "Preset which controls the speedhacks balance between accurancy and speed. This setting is applied only if 'Enable Speedhacks' option is enabled (content restart required)",
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
	"Gamefix: Align Sprite",
	"Fixes vertical lines problem in some games when resolution is upscaled.(content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
		{NULL, NULL},
	},
	"disabled" },

	{ "pcsx2_userhack_merge_sprite",
	"Gamefix: Merge Sprite",
	"Another option which could fix vertical lines problem in some games when resolution is upscaled.(content restart required)",
	{
		{"disabled", NULL},
		{"enabled", NULL},
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
