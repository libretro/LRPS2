/*  dev9null
 *  Copyright (C) 2002-2010 pcsx2 Team
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

#ifndef __DEV9_H__
#define __DEV9_H__

#define DEV9defs
#include "Pcsx2Defs.h"

typedef void (*DEV9callback)(int cycles);
typedef int (*DEV9handler)(void);

extern void (*DEV9irq)(int);

extern __aligned16 s8 dev9regs[0x10000];
#define dev9Rs8(mem) dev9regs[(mem)&0xffff]
#define dev9Rs16(mem) (*(s16 *)&dev9regs[(mem)&0xffff])
#define dev9Rs32(mem) (*(s32 *)&dev9regs[(mem)&0xffff])
#define dev9Ru8(mem) (*(u8 *)&dev9regs[(mem)&0xffff])
#define dev9Ru16(mem) (*(u16 *)&dev9regs[(mem)&0xffff])
#define dev9Ru32(mem) (*(u32 *)&dev9regs[(mem)&0xffff])

// basic funcs

// NOTE: The read/write functions CANNOT use XMM/MMX regs
// If you want to use them, need to save and restore current ones
s32 DEV9init();
s32 DEV9open();
void DEV9close();
void DEV9shutdown();

u8 DEV9read8(u32 addr);
u16 DEV9read16(u32 addr);
u32 DEV9read32(u32 addr);
void DEV9write8(u32 addr, u8 value);
void DEV9write16(u32 addr, u16 value);
void DEV9write32(u32 addr, u32 value);
void DEV9readDMA8Mem(u32 *pMem, int size);
void DEV9writeDMA8Mem(u32 *pMem, int size);

// cycles = IOP cycles before calling callback,
// if callback returns 1 the irq is triggered, else not
void DEV9irqCallback(DEV9callback callback);
DEV9handler DEV9irqHandler(void);
void DEV9async(u32 cycles);

// extended funcs

s32 CALLBACK DEV9freeze(int mode, freezeData *data);
void CALLBACK DEV9configure();

#endif
