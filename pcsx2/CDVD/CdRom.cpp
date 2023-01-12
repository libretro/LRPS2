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

#include "R3000A.h"
#include "Common.h"
#include "Utilities/MemcpyFast.h"

#include "CdRom.h"
#include "CDVD.h"
#include "IopHw.h"
#include "IopDma.h"

//THIS ALL IS FOR THE CDROM REGISTERS HANDLING

enum cdrom_registers
{
	CdlSync = 0,
	CdlNop = 1,
	CdlSetloc = 2,
	CdlPlay = 3,
	CdlForward = 4,
	CdlBackward = 5,
	CdlReadN = 6,
	CdlStandby = 7,
	CdlStop = 8,
	CdlPause = 9,
	CdlInit = 10,
	CdlMute = 11,
	CdlDemute = 12,
	CdlSetfilter = 13,
	CdlSetmode = 14,
	CdlGetmode = 15,
	CdlGetlocL = 16,
	CdlGetlocP = 17,
	Cdl18 = 18,
	CdlGetTN = 19,
	CdlGetTD = 20,
	CdlSeekL = 21,
	CdlSeekP = 22,
	CdlTest = 25,
	CdlID = 26,
	CdlReadS = 27,
	CdlReset = 28,
	CdlReadToc = 30,

	AUTOPAUSE = 249,
	READ_ACK = 250,
	READ = 251,
	REPPLAY_ACK = 252,
	REPPLAY = 253,
	ASYNC = 254
	/* don't set 255, it's reserved */
};

cdrStruct cdr;
static s32 LoadCdBios;

static u8 Test20[] = {0x98, 0x06, 0x10, 0xC3};
static u8 Test22[] = {0x66, 0x6F, 0x72, 0x20, 0x45, 0x75, 0x72, 0x6F};
static u8 Test23[] = {0x43, 0x58, 0x44, 0x32, 0x39, 0x34, 0x30, 0x51};

//backported from PCSXR
// cdr.Stat:
#define NoIntr 0
#define DataReady 1
#define Complete 2
#define Acknowledge 3
#define DataEnd 4
#define DiskError 5

/* Modes flags */
#define MODE_SPEED (1 << 7)     // 0x80
#define MODE_STRSND (1 << 6)    // 0x40 ADPCM on/off
#define MODE_SIZE_2340 (1 << 5) // 0x20
#define MODE_SIZE_2328 (1 << 4) // 0x10
#define MODE_SIZE_2048 (0 << 4) // 0x00
#define MODE_SF (1 << 3)        // 0x08 channel on/off
#define MODE_REPORT (1 << 2)    // 0x04
#define MODE_AUTOPAUSE (1 << 1) // 0x02
#define MODE_CDDA (1 << 0)      // 0x01

/* Status flags, to go on cdr.StatP */
#define STATUS_PLAY (1 << 7)      // 0x80
#define STATUS_SEEK (1 << 6)      // 0x40
#define STATUS_READ (1 << 5)      // 0x20
#define STATUS_SHELLOPEN (1 << 4) // 0x10
#define STATUS_UNKNOWN3 (1 << 3)  // 0x08
#define STATUS_UNKNOWN2 (1 << 2)  // 0x04
#define STATUS_ROTATING (1 << 1)  // 0x02
#define STATUS_ERROR (1 << 0)     // 0x01

/* Errors */
#define ERROR_NOTREADY (1 << 7)   // 0x80
#define ERROR_INVALIDCMD (1 << 6) // 0x40
#define ERROR_INVALIDARG (1 << 5) // 0x20

// 1x = 75 sectors per second
// PSXCLK = 1 sec in the ps
// so (PSXCLK / 75) / BIAS = cdr read time (linuzappz)
static u32 cdReadTime; // = ((PSXCLK / 75) / BIAS);

#define CDR_INT(eCycle) PSX_INT(IopEvt_Cdrom, eCycle)
#define CDREAD_INT(eCycle) PSX_INT(IopEvt_CdromRead, eCycle)

static const uint shortSectorSeekReadDelay = 1000; // delay for reads/seeks that may or may not have a seek action preceeding it
static uint sectorSeekReadDelay = 0x800;           // for calculated seek delays

static void AddIrqQueue(u8 irq, u32 ecycle)
{
	cdr.Irq = irq;
	if (cdr.Stat)
		cdr.eCycle = ecycle;
	else
		CDR_INT(ecycle);
}

static __fi void StartReading(u32 type)
{
	cdr.Reading = type;
	cdr.FirstSector = 1;
	cdr.Readed = 0xff;
	AddIrqQueue(READ_ACK, sectorSeekReadDelay);
	sectorSeekReadDelay = shortSectorSeekReadDelay;
}

static __fi void StopReading(void)
{
	if (cdr.Reading)
	{
		cdr.Reading = 0;
		psxRegs.interrupt &= ~(1 << IopEvt_CdromRead);
	}
}

static __fi void StopCdda(void)
{
	if (cdr.Play)
	{
		cdr.StatP &= ~STATUS_PLAY;
		cdr.Play = 0;
	}
}

static __fi void SetResultSize(u8 size)
{
	cdr.ResultP = 0;
	cdr.ResultC = size;
	cdr.ResultReady = 1;
}

static void ReadTrack(void)
{
	cdr.Prev[0] = itob(cdr.SetSector[0]);
	cdr.Prev[1] = itob(cdr.SetSector[1]);
	cdr.Prev[2] = itob(cdr.SetSector[2]);

	cdr.RErr = DoCDVDreadTrack(msf_to_lsn(cdr.SetSector), CDVD_MODE_2340);
}

void cdrInterrupt(void)
{
	cdvdTD trackInfo;
	int i;
	u8 Irq = cdr.Irq;

	if (cdr.Stat)
	{
		CDR_INT(0x800);
		return;
	}

	cdr.Irq = 0xff;
	cdr.Ctrl &= ~0x80;

	switch (Irq)
	{
		case CdlSync:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			break;

		case CdlNop:
			SetResultSize(1);
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			break;

		case CdlSetloc:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			break;

		case CdlPlay:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			cdr.StatP |= STATUS_ROTATING | STATUS_PLAY;
			break;

		case CdlForward:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlBackward:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlStandby:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlStop:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP &= ~STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			// cdr.Stat = Acknowledge;
			break;

		case CdlPause:
			SetResultSize(1);
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			AddIrqQueue(CdlPause + 0x20, 0x800);
			break;

		case CdlPause + 0x20:
			SetResultSize(1);
			cdr.StatP &= ~STATUS_READ;
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlInit:
			SetResultSize(1);
			cdr.StatP = STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			AddIrqQueue(CdlInit + 0x20, 0x800);
			break;

		case CdlInit + 0x20:
			SetResultSize(1);
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			cdr.Init = 1;
			break;

		case CdlMute:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			break;

		case CdlDemute:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			break;

		case CdlSetfilter:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			break;

		case CdlSetmode:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			break;

		case CdlGetmode:
			SetResultSize(6);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Result[1] = cdr.Mode;
			cdr.Result[2] = cdr.File;
			cdr.Result[3] = cdr.Channel;
			cdr.Result[4] = 0;
			cdr.Result[5] = 0;
			cdr.Stat = Acknowledge;
			break;

		case CdlGetlocL:
			SetResultSize(8);
			for (i = 0; i < 8; i++)
				cdr.Result[i] = cdr.Transfer[i];
			cdr.Stat = Acknowledge;
			break;

		case CdlGetlocP:
			SetResultSize(8);
			cdr.Result[0] = 1;
			cdr.Result[1] = 1;
			cdr.Result[2] = cdr.Prev[0];
			cdr.Result[3] = itob((btoi(cdr.Prev[1])) - 2);
			cdr.Result[4] = cdr.Prev[2];
			cdr.Result[5] = cdr.Prev[0];
			cdr.Result[6] = cdr.Prev[1];
			cdr.Result[7] = cdr.Prev[2];
			cdr.Stat = Acknowledge;
			break;

		case CdlGetTN:
			cdr.CmdProcess = 0;
			SetResultSize(3);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			if (CDVD->getTN(&cdr.ResultTN) == -1)
			{
				cdr.Stat = DiskError;
				cdr.Result[0] |= 0x01;
			}
			else
			{
				cdr.Stat = Acknowledge;
				cdr.Result[1] = itob(cdr.ResultTN.strack);
				cdr.Result[2] = itob(cdr.ResultTN.etrack);
			}
			break;

		case CdlGetTD:
			cdr.CmdProcess = 0;
			cdr.Track = btoi(cdr.Param[0]);
			SetResultSize(4);
			cdr.StatP |= STATUS_ROTATING;
			if (CDVD->getTD(cdr.Track, &trackInfo) == -1)
			{
				cdr.Stat = DiskError;
				cdr.Result[0] |= 0x01;
			}
			else
			{
				lsn_to_msf(cdr.ResultTD, trackInfo.lsn);
				cdr.Stat = Acknowledge;
				cdr.Result[0] = cdr.StatP;
				cdr.Result[1] = cdr.ResultTD[2];
				cdr.Result[2] = cdr.ResultTD[1];
				cdr.Result[3] = cdr.ResultTD[0];
			}
			break;

		case CdlSeekL:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			AddIrqQueue(CdlSeekL + 0x20, 0x800);
			break;

		case CdlSeekL + 0x20:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlSeekP:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			AddIrqQueue(CdlSeekP + 0x20, 0x800);
			break;

		case CdlSeekP + 0x20:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlTest:
			cdr.Stat = Acknowledge;
			switch (cdr.Param[0])
			{
				case 0x20: // System Controller ROM Version
					SetResultSize(4);
					*(int*)cdr.Result = *(int*)Test20;
					break;

				case 0x22:
					SetResultSize(8);
					*(int*)cdr.Result = *(int*)Test22;
					break;

				case 0x23:
				case 0x24:
					SetResultSize(8);
					*(int*)cdr.Result = *(int*)Test23;
					break;
			}
			break;

		case CdlID:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			AddIrqQueue(CdlID + 0x20, 0x800);
			break;

		case CdlID + 0x20:
			SetResultSize(8);
			cdr.Result[0] = 0x00; // 0x08 and cdr.Result[1]|0x10 : audio cd, enters cd player
			cdr.Result[1] = 0x00; // 0x80 leads to the menu in the bios, else loads CD

			if (!LoadCdBios)
				cdr.Result[1] |= 0x80;
			cdr.Result[2] = 0x00;
			cdr.Result[3] = 0x00;
			strncpy((char*)&cdr.Result[4], "PCSX", 4);
			cdr.Stat = Complete;
			break;

		case CdlReset:
			SetResultSize(1);
			cdr.StatP = STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			break;

		case CdlReadToc:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			AddIrqQueue(CdlReadToc + 0x20, 0x800);
			break;

		case CdlReadToc + 0x20:
			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case AUTOPAUSE:
			cdr.OCUP = 0;
			AddIrqQueue(CdlPause, 0x400);
			break;

		case READ_ACK:
			if (!cdr.Reading)
				return;

			SetResultSize(1);
			cdr.StatP |= STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;

			ReadTrack();

			CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);

			break;

		case REPPLAY_ACK:
			cdr.Stat = Acknowledge;
			cdr.Result[0] = cdr.StatP;
			SetResultSize(1);
			AddIrqQueue(REPPLAY, cdReadTime);
			break;

		case REPPLAY:
			//if ((cdr.Mode & 5) != 5) break;
			break;

		case 0xff:
			return;

		default:
			cdr.Stat = Complete;
			break;
	}

	if (cdr.Stat != NoIntr && cdr.Reg2 != 0x18)
		psxHu32(0x1070) |= 0x4;
}

void cdrReadInterrupt(void)
{
	if (!cdr.Reading)
		return;

	if (cdr.Stat)
	{                          // CDR_LOG_I("cdrom: read stat hack %02x %x\n", cdr.Irq, cdr.Stat);
		CDREAD_INT(0x800 * 4); // * 4 reduces dma3 errors lots here
		return;
	}

	cdr.OCUP = 1;
	SetResultSize(1);
	cdr.StatP |= STATUS_READ | STATUS_ROTATING;
	cdr.Result[0] = cdr.StatP;

	if (cdr.RErr == 0)
	{
		while ((cdr.RErr = DoCDVDgetBuffer(cdr.Transfer)), cdr.RErr == -2)
		{
			// not finished yet ... block on the read until it finishes.
			Threading::sleep(0);
			Threading::SpinWait();
		}
	}

	if (cdr.RErr == -1)
	{
		memzero(cdr.Transfer);
		cdr.Stat = DiskError;
		cdr.Result[0] |= STATUS_ERROR;
		ReadTrack();
		CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);
		return;
	}

	cdr.Stat = DataReady;

	cdr.SetSector[2]++;

	if (cdr.SetSector[2] == 75)
	{
		cdr.SetSector[2] = 0;
		cdr.SetSector[1]++;
		if (cdr.SetSector[1] == 60)
		{
			cdr.SetSector[1] = 0;
			cdr.SetSector[0]++;
		}
	}

	cdr.Readed = 0;

	if ((cdr.Transfer[4 + 2] & 0x80) && (cdr.Mode & 0x2))
	{ // EOF
		AddIrqQueue(CdlPause, 0x800);
	}
	else
	{
		ReadTrack();
		CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);
	}

	psxHu32(0x1070) |= 0x4;
	return;
}

/*
cdrRead0:
	bit 0 - 0 REG1 command send / 1 REG1 data read
	bit 1 - 0 data transfer finish / 1 data transfer ready/in progress
	bit 2 - unknown
	bit 3 - unknown
	bit 4 - unknown
	bit 5 - 1 result ready
	bit 6 - 1 dma ready
	bit 7 - 1 command being processed
*/

u8 cdrRead0(void)
{
	if (cdr.ResultReady)
		cdr.Ctrl |= 0x20;
	else
		cdr.Ctrl &= ~0x20;

	if (cdr.OCUP)
		cdr.Ctrl |= 0x40;
	else
		cdr.Ctrl &= ~0x40;

	// what means the 0x10 and the 0x08 bits? i only saw it used by the bios
	cdr.Ctrl |= 0x18;

	return psxHu8(0x1800) = cdr.Ctrl;
}

/*
cdrWrite0:
	0 - to send a command / 1 - to get the result
*/

void cdrWrite0(u8 rt)
{
	cdr.Ctrl = rt | (cdr.Ctrl & ~0x3);

	if (rt == 0)
	{
		cdr.ParamP = 0;
		cdr.ParamC = 0;
		cdr.ResultReady = 0;
	}
}

void setPsxSpeed(void)
{
	cdReadTime = ((PSXCLK / 75) / BIAS) * 2;
}

u8 cdrRead1(void)
{
	if (cdr.ResultReady && cdr.Ctrl & 0x1)
	{
		psxHu8(0x1801) = cdr.Result[cdr.ResultP++];
		if (cdr.ResultP == cdr.ResultC)
			cdr.ResultReady = 0;
	}
	else
		psxHu8(0x1801) = 0;

	return psxHu8(0x1801);
}

void cdrWrite1(u8 rt)
{
	int i;

	cdr.Cmd = rt;
	cdr.OCUP = 0;

	if (cdr.Ctrl & 0x1)
		return;

	switch (cdr.Cmd)
	{
		case CdlSync:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlNop:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlSetloc:
		{
			//StopReading();
			// Setloc is memorizing the wanted target, and marks it as unprocessed, and has no other effect
			// (it doesn't start reading or seeking, and doesn't interrupt or redirect any active reads).
			int oldSector = msf_to_lsn(cdr.SetSector);
			for (i = 0; i < 3; i++)
				cdr.SetSector[i] = btoi(cdr.Param[i]);
			cdr.SetSector[3] = 0;
			if ((cdr.SetSector[0] | cdr.SetSector[1] | cdr.SetSector[2]) == 0)
			{
				*(u32*)cdr.SetSector = *(u32*)cdr.SetSectorSeek;
			}
			int newSector = msf_to_lsn(cdr.SetSector);

			// sectorSeekReadDelay should lead to sensible random seek results in QA (Aging Disk) test
			sectorSeekReadDelay = abs(newSector - oldSector) * 100;
			if (sectorSeekReadDelay < shortSectorSeekReadDelay)
				sectorSeekReadDelay = shortSectorSeekReadDelay;
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800); // the seek delay occurs on the next read / seek command (CdlReadS, CdlSeekL, etc)
		}
		break;

		case CdlPlay:
			cdr.Play = 1;
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlForward:
			if (cdr.CurTrack < 0xaa)
				cdr.CurTrack++;
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlBackward:
			if (cdr.CurTrack > 1)
				cdr.CurTrack--;
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlReadN:
			cdr.Irq = 0;
			StopReading();
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			StartReading(1);
			break;

		case CdlStandby:
			StopCdda();
			StopReading();
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlStop:
			StopCdda();
			StopReading();
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlPause:
			StopCdda();
			StopReading();
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x40000);
			break;

		case CdlReset:
		case CdlInit:
			StopCdda();
			StopReading();
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlMute:
			cdr.Muted = 0;
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlDemute:
			cdr.Muted = 1;
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlSetfilter:
			cdr.File = cdr.Param[0];
			cdr.Channel = cdr.Param[1];
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlSetmode:
			cdr.Mode = cdr.Param[0];
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlGetmode:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlGetlocL:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlGetlocP:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlGetTN:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlGetTD:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlSeekL:
			((u32*)cdr.SetSectorSeek)[0] = ((u32*)cdr.SetSector)[0];
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;

			AddIrqQueue(cdr.Cmd, sectorSeekReadDelay);
			sectorSeekReadDelay = shortSectorSeekReadDelay;
			break;

		case CdlSeekP:
			((u32*)cdr.SetSectorSeek)[0] = ((u32*)cdr.SetSector)[0];
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;

			AddIrqQueue(cdr.Cmd, sectorSeekReadDelay);
			sectorSeekReadDelay = shortSectorSeekReadDelay;
			break;

		case CdlTest:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlID:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		case CdlReadS:
			cdr.Irq = 0;
			StopReading();
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			StartReading(2);
			break;

		case CdlReadToc:
			cdr.Ctrl |= 0x80;
			cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
			break;

		default:
			return;
	}
	if (cdr.Stat != NoIntr)
		iopIntcIrq(2);
}

u8 cdrRead2(void)
{
	if (cdr.Readed == 0)
		return 0;
	return *cdr.pTransfer++;
}

void cdrWrite2(u8 rt)
{
	if (cdr.Ctrl & 0x1)
	{
		switch (rt)
		{
			case 0x07:
				cdr.ParamP = 0;
				cdr.ParamC = 0;
				cdr.ResultReady = 0;
				cdr.Ctrl = 0;
				break;

			default:
				cdr.Reg2 = rt;
				break;
		}
	}
	else
	{
		if (!(cdr.Ctrl & 0x1) && cdr.ParamP < 8)
		{
			cdr.Param[cdr.ParamP++] = rt;
			cdr.ParamC++;
		}
	}
}

u8 cdrRead3(void)
{
	if (cdr.Stat)
	{
		if (cdr.Ctrl & 0x1)
			psxHu8(0x1803) = cdr.Stat | 0xE0;
		else
			psxHu8(0x1803) = 0xff;
	}
	else
		psxHu8(0x1803) = 0;

	return psxHu8(0x1803);
}

void cdrWrite3(u8 rt)
{
	if (rt == 0x07 && cdr.Ctrl & 0x1)
	{
		cdr.Stat = 0;

		if (cdr.Irq == 0xff)
		{
			cdr.Irq = 0;
			return;
		}
		if (cdr.Irq)
		{
			CDR_INT(cdr.eCycle);
		}
		return;
	}

	if (rt == 0x80 && !(cdr.Ctrl & 0x1) && cdr.Readed == 0)
	{
		cdr.Readed = 1;
		cdr.pTransfer = cdr.Transfer;

		switch (cdr.Mode & 0x30)
		{
			case 0x10:
			case 0x00:
				cdr.pTransfer += 12;
				break;
			default:
				break;
		}
	}
}

void psxDma3(u32 madr, u32 bcr, u32 chcr)
{
	u32 cdsize;

	switch (chcr)
	{
		case 0x11000000:
		case 0x11400100:
			if (cdr.Readed == 0)
			{
				HW_DMA3_CHCR &= ~0x01000000; //hack
				psxDmaInterrupt(3);          //hack
				return;
			}

			cdsize = (bcr & 0xffff) * 4;
			memcpy(iopPhysMem(madr), cdr.pTransfer, cdsize);
			psxCpu->Clear(madr, cdsize / 4);
			cdr.pTransfer += cdsize;

			break;
		case 0x41000200:
			return;
		default:
			break;
	}
	HW_DMA3_CHCR &= ~0x01000000;
	psxDmaInterrupt(3);
}

void cdrReset(void)
{
	memzero(cdr);
	cdr.CurTrack = 1;
	cdr.File = 1;
	cdr.Channel = 1;
	cdReadTime = (PSXCLK / 1757) * BIAS;
}

void SaveStateBase::cdrFreeze()
{
	FreezeTag("cdrom");
	Freeze(cdr);
}
