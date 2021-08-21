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

 * reserved keys:
	F1 to F10 are reserved for the emulator

 * plugins should NOT change the current
   working directory.
   (on win32, add flag OFN_NOCHANGEDIR for
    GetOpenFileName)

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

s32 CALLBACK GSinit();
s32 CALLBACK GSopen(const char *Title, int multithread);
s32 CALLBACK GSopen2(u32 flags);
void CALLBACK GSclose();
void CALLBACK GSshutdown();

void CALLBACK GSvsync(int field);
void CALLBACK GSgifTransfer(const u32 *pMem, u32 addr);
void CALLBACK GSgifTransfer1(u32 *pMem, u32 addr);
void CALLBACK GSgifTransfer2(u32 *pMem, u32 size);
void CALLBACK GSgifTransfer3(u32 *pMem, u32 size);
void CALLBACK GSgetLastTag(u64 *ptag); // returns the last tag processed (64 bits)
void CALLBACK GSgifSoftReset(u32 mask);
void CALLBACK GSreadFIFO(u64 *mem);
void CALLBACK GSinitReadFIFO(u64 *mem);
void CALLBACK GSreadFIFO2(u8 *mem, int qwc);
void CALLBACK GSinitReadFIFO2(u64 *mem, int qwc);

// extended funcs

void CALLBACK GSirqCallback(void (*callback)());
void CALLBACK GSsetBaseMem(void *);
void CALLBACK GSsetGameCRC(int crc, int gameoptions);

// controls frame skipping in the GS, if this routine isn't present, frame skipping won't be done
void CALLBACK GSsetFrameSkip(int frameskip);

// if start is 1, starts recording spu2 data, else stops
// returns a non zero value if successful
// for now, pData is not used
void* CALLBACK GSsetupRecording(int start);

void CALLBACK GSreset();
void CALLBACK GSwriteCSR(u32 value);
s32 CALLBACK GSfreeze(int mode, freezeData *data);
void CALLBACK GSconfigure();

#ifdef __cplusplus
} // End extern "C"
#endif

#endif /* __PS2EDEFS_H__ */
