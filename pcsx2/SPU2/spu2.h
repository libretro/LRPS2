/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "SaveState.h"

s32 SPU2init(void);
s32 SPU2reset(void);
s32 SPU2ps1reset(void);
s32 SPU2open(void);
void SPU2close(void);
void SPU2shutdown(void);
void SPU2write(u32 mem, u16 value);
u16 SPU2read(u32 mem);

void SPU2async(u32 cycles);
s32 SPU2freeze(int mode, freezeData* data);
void SPU2DoFreezeIn(pxInputStream& infp);
void SPU2DoFreezeOut(void* dest);

u32 SPU2ReadMemAddr(int core);
void SPU2WriteMemAddr(int core, u32 value);
void SPU2readDMA4Mem(u16* pMem, u32 size);
void SPU2writeDMA4Mem(u16* pMem, u32 size);
void SPU2interruptDMA4(void);
void SPU2interruptDMA7(void);
void SPU2readDMA7Mem(u16* pMem, u32 size);
void SPU2writeDMA7Mem(u16* pMem, u32 size);

extern u32 lClocks;
