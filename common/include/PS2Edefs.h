/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#ifndef __PS2EDEFS_H__
#define __PS2EDEFS_H__

/*
 *  PS2E Definitions v0.6.2 (beta)
 *
 *  Author: linuzappz@hotmail.com
 *          shadowpcsx2@yahoo.gr
 *          florinsasu@hotmail.com
 */

/*
 Notes:
 * Since this is still beta things may change.

 * OSflags:
	__linux__ (linux OS)
	_WIN32 (win32 OS)

 * common return values (for ie. GSinit):
	 0 - success
	-1 - error
*/

#include "Pcsx2Defs.h"

///////////////////////////////////////////////////////////////////////

// freeze modes:
#define FREEZE_LOAD 0
#define FREEZE_SAVE 1
#define FREEZE_SIZE 2

typedef struct
{
    int size;
    s8 *data;
} freezeData;

typedef struct _keyEvent
{
    u32 key;
    u32 evt;
} keyEvent;

///////////////////////////////////////////////////////////////////////

// key values:
/* key values must be OS dependant:
	win32: the VK_XXX will be used (WinUser)
	linux: the XK_XXX will be used (XFree86)
*/

#ifdef __cplusplus
extern "C" {
#endif

/* GS plugin API */

// basic funcs

s32 CALLBACK GSinit(void);
s32 CALLBACK GSopen2(u32 flags, u8* basemem);
void CALLBACK GSclose(void);
void CALLBACK GSshutdown(void);

void CALLBACK GSvsync(int field);
void CALLBACK GSgifTransfer(const u8 *pMem, u32 addr);
void CALLBACK GSgifTransfer1(u8 *pMem, u32 addr);
void CALLBACK GSgifTransfer2(u8 *pMem, u32 size);
void CALLBACK GSgifTransfer3(u8 *pMem, u32 size);
void CALLBACK GSgifSoftReset(u32 mask);
void CALLBACK GSInitAndReadFIFO(u8 *mem, int qwc);

// extended funcs
void CALLBACK GSsetGameCRC(int crc, int gameoptions);

void CALLBACK GSreset(void);
s32 CALLBACK GSfreeze(int mode, freezeData *data);

#ifdef __cplusplus
} // End extern "C"
#endif

#endif /* __PS2EDEFS_H__ */
