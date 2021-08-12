/*  OnePAD - author: arcum42(@gmail.com)
 *  Copyright (C) 2011
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

#ifndef __KEYSTATUS_H__
#define __KEYSTATUS_H__

#include "onepad.h"

typedef struct
{
    u8 lx, ly;
    u8 rx, ry;
} PADAnalog;

#define MAX_ANALOG_VALUE 32766

class KeyStatus
{
public:
    KeyStatus()
    {
    }

    u16 get(u32 pad);
    u8 get(u32 pad, u32 index);
};

extern KeyStatus g_key_status;

#endif
