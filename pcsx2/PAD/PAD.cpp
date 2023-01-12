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
#include <cmath>
#include "../../libretro/libretro.h"
#include "../../libretro/input.h"

#include "PS2Edefs.h"
#include "PAD.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "options_tools.h" 

#define MAX_ANALOG_VALUE 32766

typedef struct
{
    u8 lx, ly;
    u8 rx, ry;
} PADAnalog;

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

#define RUMBLE_MAX 0xFFFF

static bool rumble_enabled   = true;
static uint16_t rumble_level = 0x0;

//////////////////////////////////////////////////////////////////////
// Pad implementation
//////////////////////////////////////////////////////////////////////

/////////////////////////////////////
// State Management
/////////////////////////////////////

// Typical packet response on the bus
static const u8 ConfigExit[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const u8 noclue[7] = {0x5A, 0x00, 0x00, 0x02, 0x00, 0x00, 0x5A};
static const u8 setMode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const u8 queryModelDS2[7] = {0x5A, 0x03, 0x02, 0x00, 0x02, 0x01, 0x00};
static const u8 queryModelDS1[7] = {0x5A, 0x01, 0x02, 0x00, 0x02, 0x01, 0x00};
static const u8 queryComb[7] = {0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00};
static const u8 queryMode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const u8 setNativeMode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A};

static u8 queryMaskMode[7] = {0x5A, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x5A};

static const u8 queryAct[2][7] = {
    {0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
    {0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14}};

static QueryInfo query;
static Pad pads[2][4];
static int slots[2] = {0, 0};

static inline bool IsDualshock2(void)
{
// FIXME
#if 0
	return config.padConfigs[query.port][query.slot].type == Dualshock2Pad ||
			(config.padConfigs[query.port][query.slot].type == GuitarPad && config.GH2);
#else
    return true;
#endif
}

static int ApplyDeadZoneX(int val_x, int val_y, float deadzone_percent) 
{
	if (deadzone_percent == 0.0f) return val_x;

	float deadzone_axis = 32767.0f * deadzone_percent / 100.0f;
	float magnitude = (float)sqrt((val_x * val_x) + (val_y * val_y));

	if ((magnitude < deadzone_axis) || magnitude == 0.0f)
		return 0;
	float normalized_x = val_x / magnitude;
	int ret_val        = (int)((normalized_x * ((magnitude - deadzone_axis) / (32767.0f - deadzone_axis))) * 32767.0f);
	if (abs(ret_val) > 32767)
		ret_val = 32767 * abs(ret_val) / ret_val;
	return ret_val;
}

static int ApplyDeadZoneY(int val_x, int val_y, float deadzone_percent) 
{
	if (deadzone_percent == 0.0f) return val_y;
	float deadzone_axis = 32767.0f * deadzone_percent / 100.0f;

	float magnitude = (float)sqrt((val_x * val_x) + (val_y * val_y));

	if ((magnitude < deadzone_axis) || magnitude == 0.0f)
		return 0;
	// we keep a smooth ""kick" into motion
	float normalized_y = val_y / magnitude;
	int ret_val        = (int)((normalized_y * ((magnitude - deadzone_axis) / (32767.0f - deadzone_axis))) * 32767.0f);
	if (abs(ret_val) > 32767)
		ret_val = 32767 * abs(ret_val) / ret_val;
	return ret_val;
}



// +- 32766
static u8 key_status_get(u32 pad, u32 index)
{
	int x   = 0;
	int y   = 0;
	int val = 0;
	switch (index)
	{
		case PAD_R_LEFT:
		case PAD_R_RIGHT:
			x   = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
			y   = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
			val = ApplyDeadZoneX(x, y, option_pad_right_deadzone);
			break;

		case PAD_R_DOWN:
		case PAD_R_UP:
			x   = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
			y   = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
			val = ApplyDeadZoneY(x, y, option_pad_right_deadzone);
			break;

		case PAD_L_LEFT:
		case PAD_L_RIGHT:
			x   = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
			y   = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
			val = ApplyDeadZoneX(x, y, option_pad_left_deadzone);
			break;

		case PAD_L_DOWN:
		case PAD_L_UP:
			x   = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
			y   = input_cb(pad, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
			val = ApplyDeadZoneY(x, y, option_pad_left_deadzone);
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

void Input_Init(void)
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

void Input_Shutdown(void)
{
}

void Input_Update(void)
{
	poll_cb();
	Pad::rumble_all();
}

static void setRumbleLevel(int percent)
{
	if (percent > 100)
		percent = 100;
	else if (percent < 0)
		percent = 0;

	rumble_level = RUMBLE_MAX * percent / 100;
}

void Input_RumbleEnabled(bool enabled, int percent)
{
	rumble_enabled = enabled;
	setRumbleLevel(percent);
}

static u8 pad_start_poll(u8 pad)
{
    return query.start_poll(pad - 1);
}

static u8 pad_poll(u8 value)
{
    if (query.lastByte + 1 >= query.numBytes)
        return 0;
    if (query.lastByte && query.queryDone)
        return query.response[++query.lastByte];

    Pad *pad = &pads[query.port][query.slot];

    if (query.lastByte == 0) {
        query.lastByte++;
        query.currentCommand = value;

        switch (value) {
            case CMD_CONFIG_MODE:
                if (pad->config)
		{
                    // In config mode.  Might not actually be leaving it.
                    query.set_result(ConfigExit);
                    return 0xF3;
                }
		// fallthrough on purpose (but I don't know why)

            case CMD_READ_DATA_AND_VIBRATE: {
                query.response[2] = 0x5A;
#if 0
				int i;
				Update(query.port, query.slot);
				ButtonSum *sum = &pad->sum;

				u8 b1 = 0xFF, b2 = 0xFF;
				for (i = 0; i<4; i++) {
					b1 -= (sum->buttons[i]   > 0) << i;
				}
				for (i = 0; i<8; i++) {
					b2 -= (sum->buttons[i+4] > 0) << i;
				}
#endif

// FIXME
#if 0
				if (config.padConfigs[query.port][query.slot].type == GuitarPad && !config.GH2) {
					sum->buttons[15] = 255;
					// Not sure about this.  Forces wammy to be from 0 to 0x7F.
					// if (sum->sticks[2].vert > 0) sum->sticks[2].vert = 0;
				}
#endif

#if 0
				for (i = 4; i<8; i++) {
					b1 -= (sum->buttons[i+8] > 0) << i;
				}
#endif

// FIXME
#if 0
				//Left, Right and Down are always pressed on Pop'n Music controller.
				if (config.padConfigs[query.port][query.slot].type == PopnPad)
					b1=b1 & 0x1f;
#endif

		u16 mask         = input_cb(query.port, RETRO_DEVICE_JOYPAD,
			0, RETRO_DEVICE_ID_JOYPAD_MASK);
		uint16_t buttons = 0;
		for (int i = 0; i < 16; i++)
			buttons |= !(mask & (1 << keymap[i])) << i;

                query.numBytes = 5;

                query.response[3] = (buttons >> 8) & 0xFF;
                query.response[4] = (buttons >> 0) & 0xFF;

                if (pad->mode != MODE_DIGITAL) { // ANALOG || DS2 native
                    query.numBytes = 9;

                    query.response[5] = key_status_get(query.port, PAD_R_RIGHT);
                    query.response[6] = key_status_get(query.port, PAD_R_UP);
                    query.response[7] = key_status_get(query.port, PAD_L_RIGHT);
                    query.response[8] = key_status_get(query.port, PAD_L_UP);

                    if (pad->mode != MODE_ANALOG) { // DS2 native
                        query.numBytes = 21;

                        query.response[9]  = !(buttons & (1 << 13)) ? key_status_get(query.port, PAD_RIGHT) : 0;
                        query.response[10] = !(buttons & (1 << 15)) ? key_status_get(query.port, PAD_LEFT) : 0;
                        query.response[11] = !(buttons & (1 << 12)) ? key_status_get(query.port, PAD_UP) : 0;
                        query.response[12] = !(buttons & (1 << 14)) ? key_status_get(query.port, PAD_DOWN) : 0;

                        query.response[13] = !(buttons & (1 <<  4)) ? key_status_get(query.port, PAD_TRIANGLE) : 0;
                        query.response[14] = !(buttons & (1 <<  5)) ? key_status_get(query.port, PAD_CIRCLE) : 0;
                        query.response[15] = !(buttons & (1 <<  6)) ? key_status_get(query.port, PAD_CROSS) : 0;
                        query.response[16] = !(buttons & (1 <<  7)) ? key_status_get(query.port, PAD_SQUARE) : 0;
                        query.response[17] = !(buttons & (1 <<  2)) ? key_status_get(query.port, PAD_L1) : 0;
                        query.response[18] = !(buttons & (1 <<  3)) ? key_status_get(query.port, PAD_R1) : 0;
                        query.response[19] = !(buttons & (1 <<  0)) ? key_status_get(query.port, PAD_L2) : 0;
                        query.response[20] = !(buttons & (1 <<  1)) ? key_status_get(query.port, PAD_R2) : 0;
                    }
                }

#if 0
				query.response[3] = b1;
				query.response[4] = b2;

				query.numBytes = 5;
				if (pad->mode != MODE_DIGITAL) {
					query.response[5] = Cap((sum->sticks[0].horiz+255)/2);
					query.response[6] = Cap((sum->sticks[0].vert+255)/2);
					query.response[7] = Cap((sum->sticks[1].horiz+255)/2);
					query.response[8] = Cap((sum->sticks[1].vert+255)/2);

					query.numBytes = 9;
					if (pad->mode != MODE_ANALOG) {
						// Good idea?  No clue.
						//query.response[3] &= pad->mask[0];
						//query.response[4] &= pad->mask[1];

						// No need to cap these, already done int CapSum().
						query.response[9] = (unsigned char)sum->buttons[13]; //D-pad right
						query.response[10] = (unsigned char)sum->buttons[15]; //D-pad left
						query.response[11] = (unsigned char)sum->buttons[12]; //D-pad up
						query.response[12] = (unsigned char)sum->buttons[14]; //D-pad down

						query.response[13] = (unsigned char) sum->buttons[8];
						query.response[14] = (unsigned char) sum->buttons[9];
						query.response[15] = (unsigned char) sum->buttons[10];
						query.response[16] = (unsigned char) sum->buttons[11];
						query.response[17] = (unsigned char) sum->buttons[6];
						query.response[18] = (unsigned char) sum->buttons[7];
						query.response[19] = (unsigned char) sum->buttons[4];
						query.response[20] = (unsigned char) sum->buttons[5];
						query.numBytes = 21;
					}
				}
#endif
            }

                query.lastByte = 1;
                return pad->mode;

            case CMD_SET_VREF_PARAM:
                query.set_final_result(noclue);
                break;

            case CMD_QUERY_DS2_ANALOG_MODE:
                // Right?  Wrong?  No clue.
                if (pad->mode == MODE_DIGITAL) {
                    queryMaskMode[1] = queryMaskMode[2] = queryMaskMode[3] = 0;
                    queryMaskMode[6] = 0x00;
                } else {
                    queryMaskMode[1] = pad->umask[0];
                    queryMaskMode[2] = pad->umask[1];
                    queryMaskMode[3] = 0x03;
                    // Not entirely sure about this.
                    //queryMaskMode[3] = 0x01 | (pad->mode == MODE_DS2_NATIVE)*2;
                    queryMaskMode[6] = 0x5A;
                }
                query.set_final_result(queryMaskMode);
                break;

            case CMD_SET_MODE_AND_LOCK:
                query.set_result(setMode);
                pad->reset_vibrate();
                break;

            case CMD_QUERY_MODEL_AND_MODE:
                if (IsDualshock2()) {
                    query.set_final_result(queryModelDS2);
                } else {
                    query.set_final_result(queryModelDS1);
                }
                // Not digital mode.
                query.response[5] = (pad->mode & 0xF) != 1;
                break;

            case CMD_QUERY_ACT:
                query.set_result(queryAct[0]);
                break;

            case CMD_QUERY_COMB:
                query.set_final_result(queryComb);
                break;

            case CMD_QUERY_MODE:
                query.set_result(queryMode);
                break;

            case CMD_VIBRATION_TOGGLE:
                memcpy(query.response + 2, pad->vibrate, 7);
                query.numBytes = 9;
                //query.set_result(pad->vibrate); // warning copy 7b not 8 (but it is really important?)
                pad->reset_vibrate();
                break;

            case CMD_SET_DS2_NATIVE_MODE:
                if (IsDualshock2()) {
                    query.set_result(setNativeMode);
                } else {
                    query.set_final_result(setNativeMode);
                }
                break;

            default:
                query.numBytes = 0;
                query.queryDone = 1;
                break;
        }

        return 0xF3;

    } else {
        query.lastByte++;

        switch (query.currentCommand) {
            case CMD_READ_DATA_AND_VIBRATE:
                if (query.lastByte == pad->vibrateI[0])
                    pad->set_vibrate(1, 255 * (value & 1));
                else if (query.lastByte == pad->vibrateI[1])
                    pad->set_vibrate(0, value);

                break;

            case CMD_CONFIG_MODE:
                if (query.lastByte == 3) {
                    query.queryDone = 1;
                    pad->config = value;
                }
                break;

            case CMD_SET_MODE_AND_LOCK:
                if (query.lastByte == 3 && value < 2) {
                    pad->set_mode(value ? MODE_ANALOG : MODE_DIGITAL);
                } else if (query.lastByte == 4) {
                    if (value == 3)
                        pad->modeLock = 3;
                    else
                        pad->modeLock = 0;

                    query.queryDone = 1;
                }
                break;

            case CMD_QUERY_ACT:
                if (query.lastByte == 3) {
                    if (value < 2)
                        query.set_result(queryAct[value]);
                    // bunch of 0's
                    // else query.set_result(setMode);
                    query.queryDone = 1;
                }
                break;

            case CMD_QUERY_MODE:
                if (query.lastByte == 3 && value < 2) {
                    query.response[6] = 4 + value * 3;
                    query.queryDone = 1;
                }
                // bunch of 0's
                //else data = setMode;
                break;

            case CMD_VIBRATION_TOGGLE:
                if (query.lastByte >= 3) {
                    if (value == 0) {
                        pad->vibrateI[0] = (u8)query.lastByte;
                    } else if (value == 1) {
                        pad->vibrateI[1] = (u8)query.lastByte;
                    }
                    pad->vibrate[query.lastByte - 2] = value;
                }
                break;

            case CMD_SET_DS2_NATIVE_MODE:
                if (query.lastByte == 3 || query.lastByte == 4) {
                    pad->umask[query.lastByte - 3] = value;
                } else if (query.lastByte == 5) {
                    if (!(value & 1))
                        pad->set_mode(MODE_DIGITAL);
                    else if (!(value & 2))
                        pad->set_mode(MODE_ANALOG);
                    else
                        pad->set_mode(MODE_DS2_NATIVE);
                }
                break;

            default:
                return 0;
        }

        return query.response[query.lastByte];
    }
}


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

void PADupdate(int pad) { }

static void GamePad_DoRumble(unsigned type, unsigned pad)
{
	if (!rumble_enabled)
		return;

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

void PADconfigure(void) { }

s32 PADinit(u32 flags)
{
    Pad::reset_all();

    query.reset();

    for (int port = 0; port < 2; port++)
       slots[port] = 0;

    return 0;
}

void PADshutdown(void)
{
}

s32 PADopen(void)
{
    return 0;
}

void PADclose(void)
{
}

u32 PADquery(void)
{
    return 3; // both
}

s32 PADsetSlot(u8 port, u8 slot)
{
    port--;
    slot--;
    if (port > 1 || slot > 3)
        return 0;
    // Even if no pad there, record the slot, as it is the active slot regardless.
    slots[port] = slot;

    return 1;
}

s32 PADqueryMtap(u8 port)
{
   return 0;
}

s32 PADfreeze(int mode, freezeData *data)
{
    if (!data)
        return -1;

    if (mode == FREEZE_SIZE)
	    data->size = sizeof(PadPluginFreezeData);
    else if (mode == FREEZE_LOAD)
    {
	    PadPluginFreezeData *pdata = (PadPluginFreezeData *)(data->data);

	    Pad::stop_vibrate_all();

	    if (data->size != sizeof(PadPluginFreezeData))
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

u8 PADstartPoll(int pad)
{
    return pad_start_poll(pad);
}

u8 PADpoll(u8 value)
{
    return pad_poll(value);
}

//////////////////////////////////////////////////////////////////////
// QueryInfo implementation
//////////////////////////////////////////////////////////////////////

void QueryInfo::reset()
{
    port = 0;
    slot = 0;
    lastByte = 1;
    currentCommand = 0;
    numBytes = 0;
    queryDone = 1;
    memset(response, 0xF3, sizeof(response));
}

u8 QueryInfo::start_poll(int _port)
{
    if (port > 1) {
        reset();
        return 0;
    }

    queryDone = 0;
    port = _port;
    slot = slots[port];
    numBytes = 2;
    lastByte = 0;

    return 0xFF;
}

//////////////////////////////////////////////////////////////////////
// Pad implementation
//////////////////////////////////////////////////////////////////////

void Pad::set_mode(int _mode)
{
    mode = _mode;
}

void Pad::set_vibrate(int motor, u8 val)
{
    nextVibrate[motor] = val;
}

void Pad::reset_vibrate()
{
    set_vibrate(0, 0);
    set_vibrate(1, 0);
    memset(vibrate, 0xFF, sizeof(vibrate));
    vibrate[0] = 0x5A;
}

void Pad::reset()
{
    memset(this, 0, sizeof(PadFreezeData));

    set_mode(MODE_DIGITAL);
    umask[0] = umask[1] = 0xFF;

    // Sets up vibrate variable.
    reset_vibrate();
}

void Pad::rumble(unsigned port)
{
    for (unsigned motor = 0; motor < 2; motor++) {
        // TODO:  Probably be better to send all of these at once.
        if (nextVibrate[motor] | currentVibrate[motor]) {
            currentVibrate[motor] = nextVibrate[motor];

            if (currentVibrate[motor])
              GamePad_DoRumble(motor, port);
            else
              // Stop rumble
              GamePad_DoRumble(motor+2, port);
        }
    }
}

void Pad::stop_vibrate_all()
{
#if 0
	for (int i=0; i<8; i++) {
		SetVibrate(i&1, i>>1, 0, 0);
		SetVibrate(i&1, i>>1, 1, 0);
	}
#endif
    // FIXME equivalent ?
    for (int port = 0; port < 2; port++)
        for (int slot = 0; slot < 4; slot++)
            pads[port][slot].reset_vibrate();
}

void Pad::reset_all()
{
    for (int port = 0; port < 2; port++)
        for (int slot = 0; slot < 4; slot++)
            pads[port][slot].reset();
}

void Pad::rumble_all()
{
    for (unsigned port = 0; port < 2; port++)
        for (unsigned slot = 0; slot < 4; slot++)
            pads[port][slot].rumble(port);
}

