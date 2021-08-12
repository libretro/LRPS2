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

// for 64bit compilers
typedef char __keyEvent_Size__[(sizeof(keyEvent) == 8) ? 1 : -1];

typedef void (*DEV9callback)(int cycles);
typedef int (*DEV9handler)(void);

typedef void (*USBcallback)(int cycles);
typedef int (*USBhandler)(void);

#ifdef __cplusplus
extern "C" {
#endif

/* GS plugin API */

// basic funcs

void CALLBACK GSosdLog(const char *utf8, u32 color);
void CALLBACK GSosdMonitor(const char *key, const char *value, u32 color);

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

void CALLBACK GSchangeSaveState(int, const char *filename);
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
//deprecated: GSgetTitleInfo was used in PCSX2 but no plugin supported it prior to r4070:
//void CALLBACK GSgetTitleInfo( char dest[128] );
void CALLBACK GSgetTitleInfo2(char *dest, size_t length);
void CALLBACK GSwriteCSR(u32 value);
s32 CALLBACK GSfreeze(int mode, freezeData *data);
void CALLBACK GSconfigure();

/* PAD plugin API -=[ OBSOLETE ]=- */

// if this file is included with this define
// the next api will not be skipped by the compiler
// basic funcs

s32 CALLBACK PADinit(u32 flags);
s32 CALLBACK PADopen();
void CALLBACK PADclose();
void CALLBACK PADshutdown();
s32 CALLBACK PADfreeze(int mode, freezeData *data);


// PADkeyEvent is called every vsync (return NULL if no event)
keyEvent *CALLBACK PADkeyEvent();
u8 CALLBACK PADstartPoll(int pad);
u8 CALLBACK PADpoll(u8 value);
// returns: 1 if supported pad1
//			2 if supported pad2
//			3 if both are supported
u32 CALLBACK PADquery();

// call to give a hint to the PAD plugin to query for the keyboard state. A
// good plugin will query the OS for keyboard state ONLY in this function.
// This function is necessary when multithreading because otherwise
// the PAD plugin can get into deadlocks with the thread that really owns
// the window (and input). Note that PADupdate can be called from a different
// thread than the other functions, so mutex or other multithreading primitives
// have to be added to maintain data integrity.
void CALLBACK PADupdate(int pad);

// Send a key event from wx-gui to pad
// Note: On linux GSOpen2, wx-gui and pad share the same event buffer. Wx-gui reads and deletes event
// before the pad saw them. So the gui needs to send them back to the pad.
void CALLBACK PADWriteEvent(keyEvent &evt);

// extended funcs

s32 CALLBACK PADsetSlot(u8 port, u8 slot);
s32 CALLBACK PADqueryMtap(u8 port);
void CALLBACK PADconfigure();

/* DEV9 plugin API */

// basic funcs

// NOTE: The read/write functions CANNOT use XMM/MMX regs
// If you want to use them, need to save and restore current ones
s32 CALLBACK DEV9init();
s32 CALLBACK DEV9open();
void CALLBACK DEV9close();
void CALLBACK DEV9shutdown();
void CALLBACK DEV9keyEvent(keyEvent *ev);

u8 CALLBACK DEV9read8(u32 addr);
u16 CALLBACK DEV9read16(u32 addr);
u32 CALLBACK DEV9read32(u32 addr);
void CALLBACK DEV9write8(u32 addr, u8 value);
void CALLBACK DEV9write16(u32 addr, u16 value);
void CALLBACK DEV9write32(u32 addr, u32 value);
void CALLBACK DEV9readDMA8Mem(u32 *pMem, int size);
void CALLBACK DEV9writeDMA8Mem(u32 *pMem, int size);

// cycles = IOP cycles before calling callback,
// if callback returns 1 the irq is triggered, else not
void CALLBACK DEV9irqCallback(DEV9callback callback);
DEV9handler CALLBACK DEV9irqHandler(void);
void CALLBACK DEV9async(u32 cycles);

// extended funcs

s32 CALLBACK DEV9freeze(int mode, freezeData *data);
void CALLBACK DEV9configure();

/* USB plugin API */

// basic funcs

s32 CALLBACK USBinit();
s32 CALLBACK USBopen();
void CALLBACK USBclose();
void CALLBACK USBshutdown();
void CALLBACK USBkeyEvent(keyEvent *ev);

u8 CALLBACK USBread8(u32 addr);
u16 CALLBACK USBread16(u32 addr);
u32 CALLBACK USBread32(u32 addr);
void CALLBACK USBwrite8(u32 addr, u8 value);
void CALLBACK USBwrite16(u32 addr, u16 value);
void CALLBACK USBwrite32(u32 addr, u32 value);
void CALLBACK USBasync(u32 cycles);

// cycles = IOP cycles before calling callback,
// if callback returns 1 the irq is triggered, else not
void CALLBACK USBirqCallback(USBcallback callback);
USBhandler CALLBACK USBirqHandler(void);
void CALLBACK USBsetRAM(void *mem);

// extended funcs

s32 CALLBACK USBfreeze(int mode, freezeData *data);
void CALLBACK USBconfigure();

#ifdef __cplusplus
} // End extern "C"
#endif

#endif /* __PS2EDEFS_H__ */
