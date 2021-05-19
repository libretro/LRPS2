#include <algorithm>
#include <array>
#include <cassert>
#include <libretro.h>

#include "input.h"
#include "PS2Edefs.h"

#include "../plugins/onepad/GamePad.h"
#include "../plugins/onepad/onepad.h"
#include "../plugins/onepad/keyboard.h"
#include "../plugins/onepad/state_management.h"
#include "../plugins/onepad/KeyStatus.h"

extern retro_environment_t environ_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
struct retro_rumble_interface rumble;

static struct retro_input_descriptor desc[] = {
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Triangle"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Circle"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Cross"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Square"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L1"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R1"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
	{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
	{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "L-Analog X"},
	{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "L-Analog Y"},
	{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "R-Analog X"},
	{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "R-Analog Y"},

	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Triangle"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Circle"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Cross"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Square"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
	{1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "L-Analog X"},
	{1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "L-Analog Y"},
	{1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "R-Analog X"},
	{1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "R-Analog Y"},

	{0},
};

bool rumble_enabled = true;

namespace Input
{

void Init()
{
	environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble);
	static const struct retro_controller_description ds2_desc[] = {
		{"DualShock 2", RETRO_DEVICE_ANALOG},
	};

	static const struct retro_controller_info ports[] = {
		{ds2_desc, sizeof(ds2_desc) / sizeof(*ds2_desc)},
		{ds2_desc, sizeof(ds2_desc) / sizeof(*ds2_desc)},
		{},
	};

	environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
	//	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

void Shutdown()
{
}

void Update()
{
	poll_cb();
#ifdef __ANDROID__
	/* Android doesn't support input polling on all threads by default
   * this will force the poll for this frame to happen in the main thread
   * in case the frontend is doing late-polling */
	input_cb(0, 0, 0, 0);
#endif
	Pad::rumble_all();
}

void RumbleEnabled(bool enabled)
{
	rumble_enabled = enabled;
}


} // namespace Input

void retro_set_input_poll(retro_input_poll_t cb)
{
	poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
	input_cb = cb;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

EXPORT_C_(void)
PADabout()
{
}

EXPORT_C_(s32)
PADtest()
{
	return 0;
}

s32 _PADopen(void* pDsp)
{
	return 0;
}

void _PADclose()
{
}

EXPORT_C_(void)
PADupdate(int pad)
{
}

std::vector<std::unique_ptr<GamePad>> s_vgamePad;

void GamePad::EnumerateGamePads(std::vector<std::unique_ptr<GamePad>>& vgamePad)
{
}

void GamePad::DoRumble(unsigned type, unsigned pad)
{
	if (rumble_enabled)
	{

		if (pad >= GAMEPAD_NUMBER)
			return;

		if (type == 0)
			rumble.set_rumble_state(pad, RETRO_RUMBLE_WEAK, 0xFFFF);
		else if (type == 1)
			rumble.set_rumble_state(pad, RETRO_RUMBLE_STRONG, 0xFFFF);
		else if (type == 2)
			rumble.set_rumble_state(pad, RETRO_RUMBLE_WEAK, 0x0);
		else
			rumble.set_rumble_state(pad, RETRO_RUMBLE_STRONG, 0x0);
	}
}

EXPORT_C_(void)
PADconfigure()
{
}

void PADSaveConfig()
{
}

void PADLoadConfig()
{
}

void KeyStatus::Init()
{
}

static int keymap[] =
	{
		RETRO_DEVICE_ID_JOYPAD_L2,     // PAD_L2
		RETRO_DEVICE_ID_JOYPAD_R2,     // PAD_R2
		RETRO_DEVICE_ID_JOYPAD_L,      // PAD_L1
		RETRO_DEVICE_ID_JOYPAD_R,      // PAD_R1
		RETRO_DEVICE_ID_JOYPAD_X,      // PAD_TRIANGLE
		RETRO_DEVICE_ID_JOYPAD_A,      // PAD_CIRCLE
		RETRO_DEVICE_ID_JOYPAD_B,      // PAD_CROSS
		RETRO_DEVICE_ID_JOYPAD_Y,      // PAD_SQUARE
		RETRO_DEVICE_ID_JOYPAD_SELECT, // PAD_SELECT
		RETRO_DEVICE_ID_JOYPAD_L3,     // PAD_L3
		RETRO_DEVICE_ID_JOYPAD_R3,     // PAD_R3
		RETRO_DEVICE_ID_JOYPAD_START,  // PAD_START
		RETRO_DEVICE_ID_JOYPAD_UP,     // PAD_UP
		RETRO_DEVICE_ID_JOYPAD_RIGHT,  // PAD_RIGHT
		RETRO_DEVICE_ID_JOYPAD_DOWN,   // PAD_DOWN
		RETRO_DEVICE_ID_JOYPAD_LEFT,   // PAD_LEFT
};

u16 KeyStatus::get(u32 pad)
{
	u16 mask = input_cb(pad, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
	u16 new_mask = 0;
	for (int i = 0; i < 16; i++)
		new_mask |= !(mask & (1 << keymap[i])) << i;

	return new_mask;
}

u8 KeyStatus::get(u32 pad, u32 index)
{
	int val = 0;
	switch (index)
	{
		case PAD_R_LEFT:
		case PAD_R_RIGHT:
			val = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
			break;

		case PAD_R_DOWN:
		case PAD_R_UP:
			val = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
			break;

		case PAD_L_LEFT:
		case PAD_L_RIGHT:
			val = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
			break;

		case PAD_L_DOWN:
		case PAD_L_UP:
			val = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
			break;

		default:
			if (index < 16)
         {
				val = input_cb(pad, RETRO_DEVICE_JOYPAD, RETRO_DEVICE_INDEX_ANALOG_BUTTON, keymap[index]);
            return 0xFF - (val >> 7);
         }
			break;
	}

	return 0x80 + (val >> 8);
}
