/*  OnePAD - author: arcum42(@gmail.com)
 *  Copyright (C) 2009
 *
 *  Based on ZeroPAD, author zerofrog@gmail.com
 *  Copyright (C) 2006-2007
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <algorithm>
#include <array>
#include <cassert>
#include "../../libretro/libretro.h"
#include "../../libretro/input.h"

#include "PS2Edefs.h"
#include "onepad.h"
#include "state_management.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

const u32 revision = 2;
const u32 build = 0; // increase that with each version
#define PAD_SAVE_STATE_VERSION ((revision << 8) | (build << 0))

KeyStatus g_key_status;

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
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L1"},
	{1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R1"},
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
const uint16_t rumble_max = 0xFFFF;
uint16_t rumble_level = 0x0;

namespace Input
{

void Init()
{
	environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble);
	static const struct retro_controller_description ds2_desc[] = {
		{"DualShock 2", RETRO_DEVICE_JOYPAD},
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

void RumbleEnabled(bool enabled, int percent)
{
	rumble_enabled = enabled;
	setRumbleLevel(percent);
}

void setRumbleLevel(int percent)
{
	if (percent > 100)
		percent = 100;
	else if (percent < 0)
		percent = 0;

	rumble_level = rumble_max * percent / 100;
	
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

s32 _PADopen()
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

void GamePad::DoRumble(unsigned type, unsigned pad)
{
	if (rumble_enabled)
	{

		if (pad >= GAMEPAD_NUMBER)
			return;

		if (type == 0)
			rumble.set_rumble_state(pad, RETRO_RUMBLE_WEAK, rumble_level);
		else if (type == 1)
			rumble.set_rumble_state(pad, RETRO_RUMBLE_STRONG, rumble_level);
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
	u16 mask     = input_cb(pad, RETRO_DEVICE_JOYPAD,
			0, RETRO_DEVICE_ID_JOYPAD_MASK);
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

EXPORT_C_(s32)
PADinit(u32 flags)
{
    Pad::reset_all();

    query.reset();

    for (int port = 0; port < 2; port++)
       slots[port] = 0;

    return 0;
}

EXPORT_C_(void)
PADshutdown()
{
}

EXPORT_C_(s32)
PADopen()
{
    return _PADopen();
}

EXPORT_C_(void)
PADclose()
{
    _PADclose();
}

EXPORT_C_(u32)
PADquery()
{
    return 3; // both
}

EXPORT_C_(s32)
PADsetSlot(u8 port, u8 slot)
{
    port--;
    slot--;
    if (port > 1 || slot > 3)
        return 0;
    // Even if no pad there, record the slot, as it is the active slot regardless.
    slots[port] = slot;

    return 1;
}

EXPORT_C_(s32)
PADqueryMtap(u8 port)
{
   return 0;
}

EXPORT_C_(s32)
PADfreeze(int mode, freezeData *data)
{
    if (!data)
        return -1;

    if (mode == FREEZE_SIZE)
	    data->size = sizeof(PadPluginFreezeData);
    else if (mode == FREEZE_LOAD)
    {
	    PadPluginFreezeData *pdata = (PadPluginFreezeData *)(data->data);

	    Pad::stop_vibrate_all();

	    if (data->size != sizeof(PadPluginFreezeData) ||
			    strncmp(pdata->format, "OnePad", sizeof(pdata->format)))
		    return 0;

	    query = pdata->query;
	    if (pdata->query.slot < 4) {
		    query = pdata->query;
	    }

	    // Tales of the Abyss - pad fix
	    // - restore data for both ports
	    for (int port = 0; port < 2; port++)
	    {
		    for (int slot = 0; slot < 4; slot++)
		    {
			    u8 mode = pdata->padData[port][slot].mode;

			    if (
					    mode != MODE_DIGITAL 
					    && mode != MODE_ANALOG 
					    && mode != MODE_DS2_NATIVE)
				    break;

			    memcpy(&pads[port][slot], &pdata->padData[port][slot], sizeof(PadFreezeData));
		    }

		    if (pdata->slot[port] < 4)
			    slots[port] = pdata->slot[port];
	    }

    }
    else if (mode == FREEZE_SAVE)
    {
	    if (data->size != sizeof(PadPluginFreezeData))
		    return 0;

	    PadPluginFreezeData *pdata = (PadPluginFreezeData *)(data->data);

	    // Tales of the Abyss - pad fix
	    // - PCSX2 only saves port0 (save #1), then port1 (save #2)

	    memset(pdata, 0, data->size);
	    strncpy(pdata->format, "OnePad", sizeof(pdata->format));
	    pdata->query = query;

	    for (int port = 0; port < 2; port++)
	    {
		    for (int slot = 0; slot < 4; slot++)
			    pdata->padData[port][slot] = pads[port][slot];

		    pdata->slot[port] = slots[port];
	    }

    }
    else
        return -1;

    return 0;
}

EXPORT_C_(u8)
PADstartPoll(int pad)
{
    return pad_start_poll(pad);
}

EXPORT_C_(u8)
PADpoll(u8 value)
{
    return pad_poll(value);
}

// PADkeyEvent is called every vsync (return NULL if no event)
EXPORT_C_(keyEvent *)
PADkeyEvent()
{
    return NULL;
}
EXPORT_C_(void)
PADWriteEvent(keyEvent &evt)
{
}
