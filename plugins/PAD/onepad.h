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

#ifndef __PAD_H__
#define __PAD_H__

#define GAMEPAD_NUMBER 2 // numbers of gamepad

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <array>
#include <vector>
#include <map>
#include <string>
#include <memory>

#define PADdefs
#include "PS2Edefs.h"

enum PadOptions {
    PADOPTION_FORCEFEEDBACK = 0x1,
    PADOPTION_REVERSELX = 0x2,
    PADOPTION_REVERSELY = 0x4,
    PADOPTION_REVERSERX = 0x8,
    PADOPTION_REVERSERY = 0x10,
    PADOPTION_MOUSE_L = 0x20,
    PADOPTION_MOUSE_R = 0x40,
};

enum PadCommands {
    CMD_SET_VREF_PARAM = 0x40,
    CMD_QUERY_DS2_ANALOG_MODE = 0x41,
    CMD_READ_DATA_AND_VIBRATE = 0x42,
    CMD_CONFIG_MODE = 0x43,
    CMD_SET_MODE_AND_LOCK = 0x44,
    CMD_QUERY_MODEL_AND_MODE = 0x45,
    CMD_QUERY_ACT = 0x46,  // ??
    CMD_QUERY_COMB = 0x47, // ??
    CMD_QUERY_MODE = 0x4C, // QUERY_MODE ??
    CMD_VIBRATION_TOGGLE = 0x4D,
    CMD_SET_DS2_NATIVE_MODE = 0x4F // SET_DS2_NATIVE_MODE
};

enum gamePadValues {
    PAD_L2 = 0,   // L2 button
    PAD_R2,       // R2 button
    PAD_L1,       // L1 button
    PAD_R1,       // R1 button
    PAD_TRIANGLE, // Triangle button ▲
    PAD_CIRCLE,   // Circle button ●
    PAD_CROSS,    // Cross button ✖
    PAD_SQUARE,   // Square button ■
    PAD_SELECT,   // Select button
    PAD_L3,       // Left joystick button (L3)
    PAD_R3,       // Right joystick button (R3)
    PAD_START,    // Start button
    PAD_UP,       // Directional pad ↑
    PAD_RIGHT,    // Directional pad →
    PAD_DOWN,     // Directional pad ↓
    PAD_LEFT,     // Directional pad ←
    PAD_L_UP,     // Left joystick (Up) ↑
    PAD_L_RIGHT,  // Left joystick (Right) →
    PAD_L_DOWN,   // Left joystick (Down) ↓
    PAD_L_LEFT,   // Left joystick (Left) ←
    PAD_R_UP,     // Right joystick (Up) ↑
    PAD_R_RIGHT,  // Right joystick (Right) →
    PAD_R_DOWN,   // Right joystick (Down) ↓
    PAD_R_LEFT    // Right joystick (Left) ←
};

#include "GamePad.h"
#include "KeyStatus.h"

#define EXPORT_C_(type) extern "C" type CALLBACK
extern keyEvent event;

s32 _PADopen();
void _PADclose();
void PADsetMode(int pad, int mode);

#endif
