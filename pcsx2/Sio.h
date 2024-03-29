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

#pragma once

#include "MemoryCardFile.h"

struct _mcd
{
	u8 term; // terminator value;

	bool goodSector; // xor sector check
	u32 sectorAddr;  // read/write sector address
	u32 transferAddr; // Transfer address

	u8 FLAG;  // for PSX;
	
	u8 port; // port 
	u8 slot; // and slot for this memcard

	// Auto Eject
	u32 ForceEjection_Timeout; // in SIO checks
	wxDateTime ForceEjection_Timestamp;
};

struct _sio
{
	u16 StatReg;
	u16 ModeReg;
	u16 CtrlReg;
	u16 BaudReg;

	u32 count;     // old_sio remnant
	u32 packetsize;// old_sio remnant

	u8 buf[512];
	u8 ret; // default return value;
	u8 cmd; // command backup

	u16 bufCount; // current buffer counter
	u16 bufSize;  // supposed buffer size

	u8 port;    // current port
	u8 slot[2]; // current slot
};

extern _sio sio;

extern void sioInit(void);
extern u8 sioRead8(void);
extern void sioWrite8(u8 value);
extern void sioWriteCtrl16(u16 value);
extern void sioInterruptR(void);
extern void sioStatRead(void);
extern void sioSetGameSerial(const wxString& serial);
extern void sioNextFrame(void);
void ClearMcdEjectTimeoutNow(void);
