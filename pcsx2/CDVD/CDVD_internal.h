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

#ifndef __CDVD_INTERNAL_H__
#define __CDVD_INTERNAL_H__

/*
Interrupts - values are flag bits.

0x00	 No interrupt
0x01	 Data Ready
0x02	 Command Complete
0x03	 Acknowledge (reserved)
0x04	 End of Data Detected
0x05	 Error Detected
0x06	 Drive Not Ready

In limited experimentation I found that PS2 apps respond actively to use of the
'Data Ready' flag -- in that they'll almost immediately initiate a DMA transfer
after receiving an Irq with that as the cause.  But the question is, of course,
*when* to use it.  Adding it into some locations of CDVD reading only slowed
games down and broke things.

Using Drive Not Ready also invokes basic error handling from the Iop Bios, but
without proper emulation of the cdvd status flag it also tends to break things.

*/

enum CdvdIrqId
{
	Irq_None = 0,
	Irq_DataReady = 0,
	Irq_CommandComplete,
	Irq_Acknowledge,
	Irq_EndOfData,
	Irq_Error,
	Irq_NotReady

};

/* is cdvd.Status only for NCMDS? (linuzappz) */
/* cdvd.Status is a construction site as of now (rama)*/
enum cdvdStatus
{
	//CDVD_STATUS_NONE            = 0x00, // not sure ;)
	//CDVD_STATUS_SEEK_COMPLETE   = 0x0A,
	CDVD_STATUS_STOP      = 0x00,
	CDVD_STATUS_TRAY_OPEN = 0x01, // confirmed to be tray open
	CDVD_STATUS_SPIN      = 0x02,
	CDVD_STATUS_READ      = 0x06,
	CDVD_STATUS_PAUSE     = 0x0A, // neutral value. Recommended to never rely on this.
	CDVD_STATUS_SEEK      = 0x12,
	CDVD_STATUS_EMERGENCY = 0x20
};

enum cdvdready
{
	CDVD_NOTREADY = 0x00,
	CDVD_READY1 = 0x40,
	CDVD_READY2 = 0x4e // This is used in a few places for some reason.
					   //It would be worth checking if this was just a typo made at some point.
};

// Cdvd actions tell the emulator how and when to respond to certain requests.
// Actions are handled by the cdvdInterrupt()
enum cdvdActions
{
	cdvdAction_None = 0,
	cdvdAction_Seek,
	cdvdAction_Standby,
	cdvdAction_Stop,
	cdvdAction_Break,
	cdvdAction_Read // note: not used yet.
};

//////////////////////////////////////////////////////////////////////////////////////////
// Cdvd Block Read Cycle Timings
//
// The PS2 CDVD effectively has two seek modes -- the normal/slow one (est. avg seeks being
// around 120-160ms), and a faster seek which has an estimated seek time of about 35-40ms.
// Fast seeks happen when the destination sector is within a certain range of the starting
// point, such that abs(start-dest) is less than the value in the tbl_FastSeekDelta.
//
// CDVDs also have a secondary seeking method used when the destination is close enough
// that a contiguous sector read can reach the sector faster than initiating a full seek.
// Typically this value is very low.

enum CDVD_MODE_TYPE
{
	MODE_CDROM = 0,
	MODE_DVDROM
};

static const uint tbl_FastSeekDelta[3] = {
	4371,  // CD-ROM
	14764, // Single-layer DVD-ROM
	13360  // dual-layer DVD-ROM [currently unused]
};

// if a seek is within this many blocks, read instead of seek.
// These values are arbitrary assumptions.  Not sure what the real PS2 uses.
static const uint tbl_ContigiousSeekDelta[3] = {
	8,  // CD-ROM
	16, // single-layer DVD-ROM
	16, // dual-layer DVD-ROM [currently unused]
};

// Note: DVD read times are modified to be faster, because games seem to be a lot more
// concerned with accurate(ish) seek delays and less concerned with actual block read speeds.
// Translation: it's a minor speedhack :D

#define PSX_CD_READSPEED 153600   // 1 Byte Time @ x1 (150KB = cd x 1)
#define PSX_DVD_READSPEED 1382400 // 1 Byte Time @ x1 (1350KB = dvd x 1).

// Legacy Note: FullSeek timing causes many games to load very slow, but it likely not the real problem.
// Games breaking with it set to PSXCLK*40 : "wrath unleashed" and "Shijou Saikyou no Deshi Kenichi".

static const uint Cdvd_FullSeek_Cycles = (PSXCLK * 100) / 1000; // average number of cycles per fullseek (100ms)
static const uint Cdvd_FastSeek_Cycles = (PSXCLK * 30) / 1000;  // average number of cycles per fastseek (37ms)
								//
#endif
