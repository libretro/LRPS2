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

#define NOMINMAX

#include "Utilities/FixedPointTypes.inl"
#include "Utilities/MemcpyFast.h"
#include "R3000A.h"
#include "Common.h"
#include "IopHw.h"
#include "IopDma.h"
#include "AppConfig.h"

#include <memory>
#include <algorithm>
#include <ctype.h>
#include <wx/datetime.h>
#include <wx/wfstream.h>

#include "CdRom.h"
#include "CDVD.h"
#include "CDVD_internal.h"
#include "IsoFileFormats.h"

#include "GS.h" // for gsVideoMode
#include "Elfheader.h"
#include "ps2/BiosTools.h"

enum nCmds
{
	N_CD_SYNC = 0x00,          // CdSync
	N_CD_NOP = 0x01,           // CdNop
	N_CD_STANDBY = 0x02,       // CdStandby
	N_CD_STOP = 0x03,          // CdStop
	N_CD_PAUSE = 0x04,         // CdPause
	N_CD_SEEK = 0x05,          // CdSeek
	N_CD_READ = 0x06,          // CdRead
	N_CD_READ_CDDA = 0x07,     // CdReadCDDA
	N_DVD_READ = 0x08,         // DvdRead
	N_CD_GET_TOC = 0x09,       // CdGetToc & cdvdman_call19
	N_CMD_B = 0x0B,            // CdReadKey
	N_CD_READ_KEY = 0x0C,      // CdReadKey
	N_CD_READ_XCDDA = 0x0E,    // CdReadXCDDA
	N_CD_CHG_SPDL_CTRL = 0x0F  // CdChgSpdlCtrl
};

// NVM (eeprom) layout info
struct NVMLayout
{
	u32 biosVer;   // bios version that this eeprom layout is for
	s32 config0;   // offset of 1st config block
	s32 config1;   // offset of 2nd config block
	s32 config2;   // offset of 3rd config block
	s32 consoleId; // offset of console id (?)
	s32 ilinkId;   // offset of ilink id (ilink mac address)
	s32 modelNum;  // offset of ps2 model number (eg "SCPH-70002")
	s32 regparams; // offset of RegionParams for PStwo
	s32 mac;       // offset of the value written to 0xFFFE0188 and 0xFFFE018C on PStwo
};

#define NVM_FORMAT_MAX 2
static NVMLayout nvmlayouts[NVM_FORMAT_MAX] =
	{
		{0x000, 0x280, 0x300, 0x200, 0x1C8, 0x1C0, 0x1A0, 0x180, 0x198}, // eeproms from bios v0.00 and up
		{0x146, 0x270, 0x2B0, 0x200, 0x1C8, 0x1E0, 0x1B0, 0x180, 0x198}, // eeproms from bios v1.70 and up
};

static const char* mg_zones[8] = {"Japan", "USA", "Europe", "Oceania", "Asia", "Russia", "China", "Mexico"};

// This typically reflects the Sony-assigned serial code for the Disc, if one exists.
//  (examples:  SLUS-2113, etc).
// If the disc is homebrew then it probably won't have a valid serial; in which case
// this string will be empty.
wxString DiscSerial;

static cdvdStruct cdvd;

s64 PSXCLK = 36864000;

static __fi void SetResultSize(u8 size)
{
	cdvd.ResultC = size;
	cdvd.ResultP = 0;
	cdvd.sDataIn &= ~0x40;
}

static void CDVDREAD_INT(u32 eCycle)
{
	// Give it an arbitary FAST value. Good for ~5000kb/s in ULE when copying a file from CDVD to HDD
	// Keep long seeks out though, as games may try to push dmas while seeking. (Tales of the Abyss)
	if (EmuConfig.Speedhacks.fastCDVD)
	{
		if (eCycle < Cdvd_FullSeek_Cycles)
			eCycle = 3000;
	}

	PSX_INT(IopEvt_CdvdRead, eCycle);
}

static void CDVD_INT(int eCycle)
{
	if (eCycle == 0)
		cdvdActionInterrupt();
	else
		PSX_INT(IopEvt_Cdvd, eCycle);
}

// Sets the cdvd IRQ and the reason for the IRQ, and signals the IOP for a branch
// test (which will cause the exception to be handled).
static void cdvdSetIrq(uint id = (1 << Irq_CommandComplete))
{
	cdvd.PwOff |= id;
	iopIntcIrq(2);
	psxSetNextBranchDelta(20);
}

static int mg_BIToffset(u8* buffer)
{
	int i, ofs = 0x20;
	for (i = 0; i < *(u16*)&buffer[0x1A]; i++)
		ofs += 0x10;

	if (*(u16*)&buffer[0x18] & 1)
		ofs += buffer[ofs];
	if ((*(u16*)&buffer[0x18] & 0xF000) == 0)
		ofs += 8;

	return ofs + 0x20;
}

static void cdvdGetMechaVer(u8* ver)
{
	wxFileName mecfile(EmuConfig.BiosFilename);
	mecfile.SetExt(L"mec");
	const wxString fname(mecfile.GetFullPath());

	// Likely a bad idea to go further
	if (mecfile.IsDir())
		throw Exception::CannotCreateStream(fname);


	if (Path::GetFileSize(fname) < 4)
	{
		wxFFile fp(fname, L"wb");
		if (!fp.IsOpened())
			throw Exception::CannotCreateStream(fname);

		u8 version[4] = {0x3, 0x6, 0x2, 0x0};
		fp.Write(version, sizeof(version));
	}
}

static NVMLayout* getNvmLayout(void)
{
	if (nvmlayouts[1].biosVer <= BiosVersion)
		return &nvmlayouts[1];
	return &nvmlayouts[0];
}

// Throws Exception::CannotCreateStream if the file cannot be opened for reading, or cannot
// be created for some reason.
static void cdvdNVM(u8* buffer, int offset, size_t bytes, bool read)
{
	wxFileName nvmfile(EmuConfig.BiosFilename);
	nvmfile.SetExt(L"nvm");
	const wxString fname(nvmfile.GetFullPath());

	// Likely a bad idea to go further
	if (nvmfile.IsDir())
		throw Exception::CannotCreateStream(fname);

	if (Path::GetFileSize(fname) < 1024)
	{
		wxFFile fp(fname, L"wb");
		if (!fp.IsOpened())
			throw Exception::CannotCreateStream(fname);

		u8 zero[1024] = {0};
		fp.Write(zero, sizeof(zero));

		//Write NVM ILink area with dummy data (Age of Empires 2)

		NVMLayout* nvmLayout = getNvmLayout();
		u8 ILinkID_Data[8] = {0x00, 0xAC, 0xFF, 0xFF, 0xFF, 0xFF, 0xB9, 0x86};

		fp.Seek(*(s32*)(((u8*)nvmLayout) + offsetof(NVMLayout, ilinkId)));
		fp.Write(ILinkID_Data, sizeof(ILinkID_Data));
	}

	wxFFile fp(fname, L"r+b");
	if (!fp.IsOpened())
		throw Exception::CannotCreateStream(fname);

	fp.Seek(offset);

	if (read)
		fp.Read(buffer, bytes);
	else
		fp.Write(buffer, bytes);
}

static void cdvdReadNVM(u8* dst, int offset, int bytes)
{
	cdvdNVM(dst, offset, bytes, true);
}

static void cdvdWriteNVM(const u8* src, int offset, int bytes)
{
	cdvdNVM(const_cast<u8*>(src), offset, bytes, false);
}

void getNvmData(u8* buffer, s32 offset, s32 size, s32 fmtOffset)
{
	// find the correct bios version
	NVMLayout* nvmLayout = getNvmLayout();

	// get data from eeprom
	cdvdReadNVM(buffer, *(s32*)(((u8*)nvmLayout) + fmtOffset) + offset, size);
}

void setNvmData(const u8* buffer, s32 offset, s32 size, s32 fmtOffset)
{
	// find the correct bios version
	NVMLayout* nvmLayout = getNvmLayout();

	// set data in eeprom
	cdvdWriteNVM(buffer, *(s32*)(((u8*)nvmLayout) + fmtOffset) + offset, size);
}

static void cdvdReadConsoleID(u8* id)
{
	getNvmData(id, 0, 8, offsetof(NVMLayout, consoleId));
}
static void cdvdWriteConsoleID(const u8* id)
{
	setNvmData(id, 0, 8, offsetof(NVMLayout, consoleId));
}

static void cdvdReadILinkID(u8* id)
{
	getNvmData(id, 0, 8, offsetof(NVMLayout, ilinkId));
}
static void cdvdWriteILinkID(const u8* id)
{
	setNvmData(id, 0, 8, offsetof(NVMLayout, ilinkId));
}

static void cdvdReadModelNumber(u8* num, s32 part)
{
	getNvmData(num, part, 8, offsetof(NVMLayout, modelNum));
}

static void cdvdWriteModelNumber(const u8* num, s32 part)
{
	setNvmData(num, part, 8, offsetof(NVMLayout, modelNum));
}

static void cdvdReadRegionParams(u8* num)
{
	getNvmData(num, 0, 8, offsetof(NVMLayout, regparams));
}

static void cdvdWriteRegionParams(const u8* num)
{
	setNvmData(num, 0, 8, offsetof(NVMLayout, regparams));
}

static void cdvdReadMAC(u8* num)
{
	getNvmData(num, 0, 8, offsetof(NVMLayout, mac));
}

static void cdvdWriteMAC(const u8* num)
{
	setNvmData(num, 0, 8, offsetof(NVMLayout, mac));
}

void cdvdReadLanguageParams(u8* config)
{
	getNvmData(config, 0xF, 16, offsetof(NVMLayout, config1));
}

s32 cdvdReadConfig(u8* config)
{
	// make sure its in read mode
	if (cdvd.CReadWrite != 0)
	{
		config[0] = 0x80;
		memset(&config[1], 0x00, 15);
		return 1;
	}
	// check if block index is in bounds
	else if (cdvd.CBlockIndex >= cdvd.CNumBlocks)
		return 1;
	else if (
		((cdvd.COffset == 0) && (cdvd.CBlockIndex >= 4)) ||
		((cdvd.COffset == 1) && (cdvd.CBlockIndex >= 2)) ||
		((cdvd.COffset == 2) && (cdvd.CBlockIndex >= 7)))
		memset(config, 0, 16);
	else
	{
		// get config data
		switch (cdvd.COffset)
		{
			case 0:
				getNvmData(config, (cdvd.CBlockIndex++) * 16, 16, offsetof(NVMLayout, config0));
				break;
			case 2:
				getNvmData(config, (cdvd.CBlockIndex++) * 16, 16, offsetof(NVMLayout, config2));
				break;
			default:
				getNvmData(config, (cdvd.CBlockIndex++) * 16, 16, offsetof(NVMLayout, config1));
		}
	}
	return 0;
}

s32 cdvdWriteConfig(const u8* config)
{
	// make sure its in write mode && the block index is in bounds
	if ((cdvd.CReadWrite != 1) || (cdvd.CBlockIndex >= cdvd.CNumBlocks))
		return 1;
	else if (
		((cdvd.COffset == 0) && (cdvd.CBlockIndex >= 4)) ||
		((cdvd.COffset == 1) && (cdvd.CBlockIndex >= 2)) ||
		((cdvd.COffset == 2) && (cdvd.CBlockIndex >= 7)))
		return 0;

	// get config data
	switch (cdvd.COffset)
	{
		case 0:
			setNvmData(config, (cdvd.CBlockIndex++) * 16, 16, offsetof(NVMLayout, config0));
			break;
		case 2:
			setNvmData(config, (cdvd.CBlockIndex++) * 16, 16, offsetof(NVMLayout, config2));
			break;
		default:
			setNvmData(config, (cdvd.CBlockIndex++) * 16, 16, offsetof(NVMLayout, config1));
	}
	return 0;
}

// Sets ElfCRC to the CRC of the game bound to the CDVD source.
static __fi ElfObject* loadElf(const wxString filename)
{
	if (filename.StartsWith(L"host"))
		return new ElfObject(filename.After(':'), Path::GetFileSize(filename.After(':')));

	// Mimic PS2 behavior!
	// Much trial-and-error with changing the ISOFS and BOOT2 contents of an image have shown that
	// the PS2 BIOS performs the peculiar task of *ignoring* the version info from the parsed BOOT2
	// filename *and* the ISOFS, when loading the game's ELF image.  What this means is:
	//
	//   1. a valid PS2 ELF can have any version (ISOFS), and the version need not match the one in SYSTEM.CNF.
	//   2. the version info on the file in the BOOT2 parameter of SYSTEM.CNF can be missing, 10 chars long,
	//      or anything else.  Its all ignored.
	//   3. Games loading their own files do *not* exhibit this behavior; likely due to using newer IOP modules
	//      or lower level filesystem APIs (fortunately that doesn't affect us).
	//
	// FIXME: Properly mimicing this behavior is troublesome since we need to add support for "ignoring"
	// version information when doing file searches.  I'll add this later.  For now, assuming a ;1 should
	// be sufficient (no known games have their ELF binary as anything but version ;1)

	const wxString fixedname(wxStringTokenizer(filename, L';').GetNextToken() + L";1");
	IsoFSCDVD isofs;
	IsoFile file(isofs, fixedname);
	return new ElfObject(fixedname, file);
}

static __fi void _reloadElfInfo(wxString elfpath)
{
	if (elfpath == LastELF)
		return;
	LastELF = elfpath;

	wxString fname = elfpath.AfterLast('\\');
	if (!fname)
		fname = elfpath.AfterLast('/');
	if (!fname)
		fname = elfpath.AfterLast(':');
	if (fname.Matches(L"????_???.??*"))
		DiscSerial = fname(0, 4) + L"-" + fname(5, 3) + fname(9, 2);

	std::unique_ptr<ElfObject> elfptr(loadElf(elfpath));

	elfptr->loadHeaders();
	ElfCRC = elfptr->getCRC();
	ElfEntry = elfptr->header.e_entry;
	ElfTextRange = elfptr->getTextRange();

	// Note: Do not load game database info here.  This code is generic and called from
	// BIOS key encryption as well as eeloadReplaceOSDSYS.  The first is actually still executing
	// BIOS code, and patches and cheats should not be applied yet.  (they are applied when
	// eeGameStarting is invoked, which is when the VM starts executing the actual game ELF
	// binary).
}

void cdvdReloadElfInfo(wxString elfoverride)
{
	// called from context of executing VM code (recompilers), 
	// so we need to trap exceptions
	// and route them through the VM's exception handler.

	try
	{
		if (!elfoverride.IsEmpty())
		{
			_reloadElfInfo(elfoverride);
			return;
		}

		wxString elfpath;
		u32 discType = GetPS2ElfName(elfpath);

		if (discType == 1)
		{
			// PCSX2 currently only recognizes *.elf executables in proper PS2 format.
			// To support different PSX titles in the console title and for savestates, this code bypasses all the detection,
			// simply using the exe name, stripped of problematic characters.
			wxString fname = elfpath.AfterLast('\\').AfterLast(':'); // Also catch elf paths which lack a backslash, and only have a colon.
			wxString fname2 = fname.BeforeFirst(';');
			DiscSerial = fname2;
			return;
		}

		// Isn't a disc we recognize?
		if (discType == 0)
			return;

		// Recognized and PS2 (BOOT2).  Good job, user.
		_reloadElfInfo(elfpath);
	}
	catch (Exception::FileNotFound& e) { }
}

static __fi s32 StrToS32(const wxString& str, int base = 10)
{
	long l;
	if (!str.ToLong(&l, base))
		return 0;
	return l;
}

static void cdvdReadKey(u8, u16, u32 arg2, u8* key)
{
	s32 numbers = 0, letters = 0;
	u32 key_0_3;
	u8 key_4, key_14;

	cdvdReloadElfInfo(wxEmptyString);

	// clear key values
	memset(key, 0, 16);

	if (!DiscSerial.IsEmpty())
	{
		// convert the number characters to a real 32 bit number
		numbers = StrToS32(DiscSerial(5, 5));

		// combine the lower 7 bits of each char
		// to make the 4 letters fit into a single u32
		letters = (s32)((DiscSerial[3].GetValue() & 0x7F) << 0) |
				  (s32)((DiscSerial[2].GetValue() & 0x7F) << 7) |
				  (s32)((DiscSerial[1].GetValue() & 0x7F) << 14) |
				  (s32)((DiscSerial[0].GetValue() & 0x7F) << 21);
	}

	// calculate magic numbers
	key_0_3 = ((numbers & 0x1FC00) >> 10) | ((0x01FFFFFF & letters) << 7);   // numbers = 7F  letters = FFFFFF80
	key_4   = ((numbers & 0x0001F) << 3) | ((0x0E000000 & letters) >> 25);   // numbers = F8  letters = 07
	key_14  = ((numbers & 0x003E0) >> 2) | 0x04;                             // numbers = F8  extra   = 04  unused = 03

	// store key values
	key[0]  = (key_0_3 & 0x000000FF) >> 0;
	key[1]  = (key_0_3 & 0x0000FF00) >> 8;
	key[2]  = (key_0_3 & 0x00FF0000) >> 16;
	key[3]  = (key_0_3 & 0xFF000000) >> 24;
	key[4]  = key_4;

	switch (arg2)
	{
		case 75:
			key[14] = key_14;
			key[15] = 0x05;
			break;

		case 4246:
			// 0x0001F2F707 = sector 0x0001F2F7  dec 0x07
			key[0] = 0x07;
			key[1] = 0xF7;
			key[2] = 0xF2;
			key[3] = 0x01;
			key[4] = 0x00;
			key[15] = 0x01;
			break;

		default:
			key[15] = 0x01;
			break;
	}
}

static s32 cdvdGetToc(void* toc)
{
	s32 ret = CDVD->getTOC(toc);
	if (ret == -1)
		return 0x80;
	return ret;
}

static s32 cdvdReadSubQ(s32 lsn, cdvdSubQ* subq)
{
	s32 ret = CDVD->readSubQ(lsn, subq);
	if (ret == -1)
		return 0x80;
	return ret;
}

static void cdvdDetectDisk(void)
{
	cdvd.Type = DoCDVDdetectDiskType();
	cdvdReloadElfInfo(wxEmptyString);
}

s32 cdvdCtrlTrayOpen(void)
{
	cdvd.Status = CDVD_STATUS_TRAY_OPEN;
	cdvd.Ready = CDVD_NOTREADY;

	return 0; // needs to be 0 for success according to homebrew test "CDVD"
}

s32 cdvdCtrlTrayClose(void)
{
	cdvd.Status = CDVD_STATUS_PAUSE;
	cdvd.Ready = CDVD_READY1;
	cdvd.TrayTimeout = 0; // Reset so it can't get closed twice by cdvdVsync()

	cdvdDetectDisk();
	GetCoreThread().ApplySettings(g_Conf->EmuOptions);

	return 0; // needs to be 0 for success according to homebrew test "CDVD"
}

// check whether disc is single or dual layer
// if its dual layer, check what the disctype is and what sector number
// layer1 starts at
//
// args:    gets value for dvd type (0=single layer, 1=ptp, 2=otp)
//          gets value for start lsn of layer1
// returns: 1 if on dual layer disc
//          0 if not on dual layer disc
static s32 cdvdReadDvdDualInfo(s32* dualType, u32* layer1Start)
{
	*dualType = 0;
	*layer1Start = 0;

	return CDVD->getDualInfo(dualType, layer1Start);
}

static uint cdvdBlockReadTime(CDVD_MODE_TYPE mode)
{
	int numSectors = 0;
	int offset = 0;
	// Sector counts are taken from google for Single layer, Dual layer DVD's and for 700MB CD's
	switch (cdvd.Type)
	{
	case CDVD_TYPE_DETCTDVDS:
	case CDVD_TYPE_PS2DVD:
		numSectors = 2298496;
		break;
	case CDVD_TYPE_DETCTDVDD:
		numSectors = 4173824 / 2; // Total sectors for both layers, assume half per layer
		u32 layer1Start;
		s32 dualType;

		// Layer 1 needs an offset as it goes back to the middle of the disc
		cdvdReadDvdDualInfo(&dualType, &layer1Start);
		if (cdvd.Sector >= layer1Start)
			offset = layer1Start;
		break;
	default: // Pretty much every CD format
		numSectors = 360000;
		break;
	}
	// Read speed is roughly 37% at lowest and full speed on outer edge. I imagine it's more logarithmic than this
	// Required for Shadowman to work
	// Use SeekToSector as Sector hasn't been updated yet
	const float sectorSpeed = (((float)(cdvd.SeekToSector - offset) / numSectors) * 0.63f) + 0.37f;
	return ((PSXCLK * cdvd.BlockSize) / ((float)(((mode == MODE_CDROM) ? PSX_CD_READSPEED : PSX_DVD_READSPEED) * cdvd.Speed) * sectorSpeed));
}

void cdvdReset(void)
{
	memzero(cdvd);

	cdvd.Type = CDVD_TYPE_NODISC;
	cdvd.Spinning = false;

	cdvd.sDataIn = 0x40;
	cdvd.Ready = CDVD_READY2;
	cdvd.Speed = 4;
	cdvd.BlockSize = 2064;
	cdvd.Action = cdvdAction_None;
	cdvd.ReadTime = cdvdBlockReadTime(MODE_DVDROM);

	// CDVD internally uses GMT+9.  If you think the time's wrong, you're wrong.
	// Set up your time zone and winter/summer in the BIOS.  No PS2 BIOS I know of features automatic DST.
	wxDateTime curtime(wxDateTime::GetTimeNow());
	cdvd.RTC.second = (u8)curtime.GetSecond();
	cdvd.RTC.minute = (u8)curtime.GetMinute();
	cdvd.RTC.hour = (u8)curtime.GetHour(wxDateTime::GMT9);
	cdvd.RTC.day = (u8)curtime.GetDay(wxDateTime::GMT9);
	cdvd.RTC.month = (u8)curtime.GetMonth(wxDateTime::GMT9) + 1; // WX returns Jan as "0"
	cdvd.RTC.year = (u8)(curtime.GetYear(wxDateTime::GMT9) - 2000);
}

struct Freeze_v10Compat
{
	u8 Action;
	u32 SeekToSector;
	u32 ReadTime;
	bool Spinning;
};

void SaveStateBase::cdvdFreeze()
{
	FreezeTag("cdvd");
	Freeze(cdvd);

	if (IsLoading())
	{
		// Make sure the Cdvd source has the expected track loaded into the buffer.
		// If cdvd.Readed is cleared it means we need to load the SeekToSector (ie, a
		// seek is in progress!)

		if (cdvd.Reading)
			cdvd.RErr = DoCDVDreadTrack(cdvd.Readed ? cdvd.Sector : cdvd.SeekToSector, cdvd.ReadMode);
	}
}

void cdvdNewDiskCB(void)
{
	DoCDVDresetDiskTypeCache();
	cdvdDetectDisk();
}

static void mechaDecryptBytes(u32 madr, int size)
{
	int shiftAmount = (cdvd.decSet >> 4) & 7;
	int doXor       = (cdvd.decSet) & 1;
	int doShift     = (cdvd.decSet) & 2;
	u8* curval      = iopPhysMem(madr);
	for (int i = 0; i < size; ++i, ++curval)
	{
		if (doXor)
			*curval ^= cdvd.Key[4];
		if (doShift)
			*curval = (*curval >> shiftAmount) | (*curval << (8 - shiftAmount));
	}
}

int cdvdReadSector(void)
{
	s32 bcr = (HW_DMA3_BCR_H16 * HW_DMA3_BCR_L16) * 4;
	if (bcr < cdvd.BlockSize)
	{
		if (HW_DMA3_CHCR & 0x01000000)
		{
			HW_DMA3_CHCR &= ~0x01000000;
			psxDmaInterrupt(3);
		}
		return -1;
	}

	// DMAs use physical addresses (air)
	u8* mdest = iopPhysMem(HW_DMA3_MADR);

	// if raw dvd sector 'fill in the blanks'
	if (cdvd.BlockSize == 2064)
	{
		// get info on dvd type and layer1 start
		u32 layer1Start;
		s32 dualType;
		s32 layerNum;
		u32 lsn = cdvd.Sector;

		cdvdReadDvdDualInfo(&dualType, &layer1Start);

		if ((dualType == 1) && (lsn >= layer1Start))
		{
			// dual layer ptp disc
			layerNum = 1;
			lsn = lsn - layer1Start + 0x30000;
		}
		else if ((dualType == 2) && (lsn >= layer1Start))
		{
			// dual layer otp disc
			layerNum = 1;
			lsn = ~(layer1Start + 0x30000 - 1);
		}
		else
		{
			// Assuming the other dualType is 0,
			// single layer disc, or on first layer of dual layer disc.
			layerNum = 0;
			lsn += 0x30000;
		}

		mdest[0] = 0x20 | layerNum;
		mdest[1] = (u8)(lsn >> 16);
		mdest[2] = (u8)(lsn >> 8);
		mdest[3] = (u8)(lsn);

		// sector IED (not calculated at present)
		mdest[4] = 0;
		mdest[5] = 0;

		// sector CPR_MAI (not calculated at present)
		mdest[6] = 0;
		mdest[7] = 0;
		mdest[8] = 0;
		mdest[9] = 0;
		mdest[10] = 0;
		mdest[11] = 0;

		// normal 2048 bytes of sector data
		memcpy(&mdest[12], cdr.Transfer, 2048);

		// 4 bytes of edc (not calculated at present)
		mdest[2060] = 0;
		mdest[2061] = 0;
		mdest[2062] = 0;
		mdest[2063] = 0;
	}
	else
		memcpy(mdest, cdr.Transfer, cdvd.BlockSize);

	// decrypt sector's bytes
	if (cdvd.decSet)
		mechaDecryptBytes(HW_DMA3_MADR, cdvd.BlockSize);

	// Added a clear after memory write .. never seemed to be necessary before but *should*
	// be more correct. (air)
	psxCpu->Clear(HW_DMA3_MADR, cdvd.BlockSize / 4);

	HW_DMA3_BCR_H16 -= (cdvd.BlockSize / (HW_DMA3_BCR_L16 * 4));
	HW_DMA3_MADR += cdvd.BlockSize;

	return 0;
}

// inlined due to being referenced in only one place.
__fi void cdvdActionInterrupt()
{
	switch (cdvd.Action)
	{
		case cdvdAction_Seek:
			cdvd.Spinning = true;
			cdvd.Ready = CDVD_READY1; //check (rama)
			cdvd.Sector = cdvd.SeekToSector;
			cdvd.Status = CDVD_STATUS_PAUSE;
			break;

		case cdvdAction_Standby:
			cdvd.Spinning = true;     //check (rama)
			cdvd.Ready = CDVD_READY1; //check (rama)
			cdvd.Sector = cdvd.SeekToSector;
			cdvd.Status = CDVD_STATUS_PAUSE;
			break;

		case cdvdAction_Stop:
			cdvd.Spinning = false;
			cdvd.Ready = CDVD_READY1;
			cdvd.Sector = 0;
			cdvd.Status = CDVD_STATUS_STOP;
			break;

		case cdvdAction_Break:
			// Make sure the cdvd action state is pretty well cleared:
			cdvd.Reading = 0;
			cdvd.Readed = 0;
			cdvd.Ready = CDVD_READY2;        // should be CDVD_READY1 or something else?
			cdvd.Status = CDVD_STATUS_PAUSE; //Break stops the command in progress it doesn't stop the drive. Formula 2001
			cdvd.RErr = 0;
			cdvd.nCommand = 0;
			break;
	}
	cdvd.Action = cdvdAction_None;

	cdvd.PwOff |= 1 << Irq_CommandComplete;
	psxHu32(0x1070) |= 0x4;
}

// inlined due to being referenced in only one place.
__fi void cdvdReadInterrupt()
{
	cdvd.Ready = CDVD_NOTREADY;
	if (!cdvd.Readed)
	{
		// Seeking finished.  Process the track we requested before, and
		// then schedule another CDVD read int for when the block read finishes.

		// NOTE: The first CD track was read when the seek was initiated, so no need
		// to call CDVDReadTrack here.

		cdvd.Spinning = true;
		cdvd.RetryCntP = 0;
		cdvd.Reading = 1;
		cdvd.Readed = 1;
		cdvd.Status = CDVD_STATUS_READ | CDVD_STATUS_SPIN; // Time Crisis 2

		cdvd.Sector = cdvd.SeekToSector;

		CDVDREAD_INT(cdvd.ReadTime);
		return;
	}
	else
	{
		if (cdvd.RErr == 0)
		{
			while ((cdvd.RErr = DoCDVDgetBuffer(cdr.Transfer)), cdvd.RErr == -2)
			{
				// not finished yet ... block on the read until it finishes.
				Threading::sleep(0);
				Threading::SpinWait();
			}
		}

		if (cdvd.RErr == -1)
		{
			cdvd.RetryCntP++;

			if (cdvd.RetryCntP <= cdvd.RetryCnt)
			{
				cdvd.RErr = DoCDVDreadTrack(cdvd.Sector, cdvd.ReadMode);
				CDVDREAD_INT(cdvd.ReadTime);
			}

			return;
		}

		cdvd.Reading = false;
	}

	if (cdvd.nSectors > 0)
	{
		if (cdvdReadSector() == -1)
		{
			// This means that the BCR/DMA hasn't finished yet, and rather than fire off the
			// sector-finished notice too early (which might overwrite game data) we delay a
			// bit and try to read the sector again later.
			// An arbitrary delay of some number of cycles probably makes more sense here,
			// but for now it's based on the cdvd.ReadTime value. -- air

			CDVDREAD_INT(cdvd.ReadTime / 4);
			return;
		}

		cdvd.Sector++;
	}

	if (--cdvd.nSectors <= 0)
	{
		// Setting the data ready flag fixes a black screen loading issue in
		// Street Fighter Ex3 (NTSC-J version).
		cdvd.PwOff |= (1 << Irq_DataReady) | (1 << Irq_CommandComplete);
		psxHu32(0x1070) |= 0x4;

		HW_DMA3_CHCR &= ~0x01000000;
		psxDmaInterrupt(3);
		cdvd.Ready = CDVD_READY2;
		cdvd.Status = CDVD_STATUS_PAUSE; // Needed here but could be smth else than Pause (rama)
		// All done! :D
		return;
	}

	cdvd.RetryCntP = 0;
	cdvd.Reading = 1;
	cdvd.RErr = DoCDVDreadTrack(cdvd.Sector, cdvd.ReadMode);
	CDVDREAD_INT(cdvd.ReadTime);

	return;
}

// Returns the number of IOP cycles until the event completes.
static uint cdvdStartSeek(uint newsector, CDVD_MODE_TYPE mode)
{
	uint seektime, delta;
	cdvd.SeekToSector = newsector;

	delta             = abs((s32)(cdvd.SeekToSector - cdvd.Sector));

	cdvd.Ready        = CDVD_NOTREADY;
	cdvd.Reading      = 0;
	cdvd.Readed       = 0;
	cdvd.Status       = CDVD_STATUS_PAUSE; // best so far in my tests (rama)

	if (!cdvd.Spinning)
	{
		seektime      = PSXCLK / 3; // 333ms delay
		cdvd.Spinning = true;
	}
	else if ((tbl_ContigiousSeekDelta[mode] == 0) || (delta >= tbl_ContigiousSeekDelta[mode]))
	{
		// Select either Full or Fast seek depending on delta:

		// Full Seek
		if (delta >= tbl_FastSeekDelta[mode])
			seektime = Cdvd_FullSeek_Cycles;
		else
			seektime = Cdvd_FastSeek_Cycles;
	}
	else
	{
		// seektime is the time it takes to read to the destination block:
		seektime = delta * cdvd.ReadTime;

		if (delta == 0)
		{
			//cdvd.Status = CDVD_STATUS_PAUSE;
			cdvd.Status = CDVD_STATUS_READ | CDVD_STATUS_SPIN; // Time Crisis 2
			cdvd.Readed = 1;                                   // Note: 1, not 0, as implied by the next comment. Need to look into this. --arcum42
			cdvd.RetryCntP = 0;

			// setting Readed to 0 skips the seek logic, which means the next call to
			// cdvdReadInterrupt will load a block.  So make sure it's properly scheduled
			// based on sector read speeds:

			seektime = cdvd.ReadTime;
		}
	}

	return seektime;
}

u8 monthmap[13] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void cdvdVsync()
{
	cdvd.RTCcount++;
	if (cdvd.RTCcount < (GetVerticalFrequency().ToIntRounded()))
		return;
	cdvd.RTCcount = 0;

	if (cdvd.Status == CDVD_STATUS_TRAY_OPEN)
		cdvd.TrayTimeout++;
	if (cdvd.TrayTimeout > 3)
	{
		cdvdCtrlTrayClose();
		cdvd.TrayTimeout = 0;
	}

	cdvd.RTC.second++;
	if (cdvd.RTC.second < 60)
		return;
	cdvd.RTC.second = 0;

	cdvd.RTC.minute++;
	if (cdvd.RTC.minute < 60)
		return;
	cdvd.RTC.minute = 0;

	cdvd.RTC.hour++;
	if (cdvd.RTC.hour < 24)
		return;
	cdvd.RTC.hour = 0;

	cdvd.RTC.day++;
	if (cdvd.RTC.day <= (cdvd.RTC.month == 2 && cdvd.RTC.year % 4 == 0 ? 29 : monthmap[cdvd.RTC.month - 1]))
		return;
	cdvd.RTC.day = 1;

	cdvd.RTC.month++;
	if (cdvd.RTC.month <= 12)
		return;
	cdvd.RTC.month = 1;

	cdvd.RTC.year++;
	if (cdvd.RTC.year < 100)
		return;
	cdvd.RTC.year = 0;
}

static __fi u8 cdvdRead18(void) // SDATAOUT
{
	u8 ret = 0;

	if (((cdvd.sDataIn & 0x40) == 0) && (cdvd.ResultP < cdvd.ResultC))
	{
		cdvd.ResultP++;
		if (cdvd.ResultP >= cdvd.ResultC)
			cdvd.sDataIn |= 0x40;
		ret = cdvd.Result[cdvd.ResultP - 1];
	}

	return ret;
}

u8 cdvdRead(u8 key)
{
	switch (key)
	{
		case 0x04: // NCOMMAND
			return cdvd.nCommand;

		case 0x05: // N-READY
			return cdvd.Ready;

		case 0x06: // ERROR
			return cdvd.Error;


		case 0x08: // INTR_STAT
			return cdvd.PwOff;

		case 0x0A: // STATUS
			return cdvd.Status;

		case 0x0B: // TRAY-STATE (if tray has been opened)
			if (cdvd.Status == CDVD_STATUS_TRAY_OPEN)
				return 1;
			/* fall-through */
		case 0x07: // BREAK
			return 0;

		case 0x0C: // CRT MINUTE
			return itob((u8)(cdvd.Sector / (60 * 75)));

		case 0x0D: // CRT SECOND
			return itob((u8)((cdvd.Sector / 75) % 60) + 2);

		case 0x0E: // CRT FRAME
			return itob((u8)(cdvd.Sector % 75));

		case 0x0F: // TYPE
			cdvd.Type = DoCDVDdetectDiskType();
			return cdvd.Type;

		case 0x13: // UNKNOWN
			return 4;

		case 0x15: // RSV
			return 0x01; // | 0x80 for ATAPI mode

		case 0x16: // SCOMMAND
			return cdvd.sCommand;

		case 0x17: // SREADY
			return cdvd.sDataIn;

		case 0x18:
			return cdvdRead18();

		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		{
			int temp = key - 0x20;

			return cdvd.Key[temp];
		}
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B:
		case 0x2C:
		{
			int temp = key - 0x23;

			return cdvd.Key[temp];
		}

		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		{
			int temp = key - 0x26;

			return cdvd.Key[temp];
		}

		case 0x38: // valid parts of key data (first and last are valid)

			return cdvd.Key[15];

		case 0x39: // KEY-XOR
			return cdvd.KeyXor;

		case 0x3A: // DEC_SET
			return cdvd.decSet;

		default:
			// note: notify the console since this is a potentially serious emulation problem:
			break;
	}

	return -1;
}

static void cdvdWrite04(u8 rt)
{ // NCOMMAND
	cdvd.nCommand = rt;
	// Why fiddle with Status and PwOff here at all? (rama)
	cdvd.Status = cdvd.Spinning ? CDVD_STATUS_SPIN : CDVD_STATUS_STOP; // checkme
	cdvd.PwOff = Irq_None;                                             // good or bad?

	switch (rt)
	{
		case N_CD_SYNC: // CdSync
		case N_CD_NOP:  // CdNop_
			cdvdSetIrq();
			break;

		case N_CD_STANDBY: // CdStandby

			// Seek to sector zero.  The cdvdStartSeek function will simulate
			// spinup times if needed.

			cdvd.Action = cdvdAction_Standby;
			cdvd.ReadTime = cdvdBlockReadTime(MODE_DVDROM);
			CDVD_INT(cdvdStartSeek(0, MODE_DVDROM));
			break;

		case N_CD_STOP: // CdStop
			cdvd.Action = cdvdAction_Stop;
			CDVD_INT(PSXCLK / 6); // 166ms delay?
			break;

		// from an emulation point of view there is not much need to do anything for this one
		case N_CD_PAUSE: // CdPause
			cdvdSetIrq();
			break;

		case N_CD_SEEK: // CdSeek
			cdvd.Action = cdvdAction_Seek;
			cdvd.ReadTime = cdvdBlockReadTime(MODE_DVDROM);
			CDVD_INT(cdvdStartSeek(*(uint*)(cdvd.Param + 0), MODE_DVDROM));
			break;

		case N_CD_READ: // CdRead
			// Assign the seek to sector based on cdvd.Param[0]-[3], and the number of  sectors based on cdvd.Param[4]-[7].
			cdvd.SeekToSector = *(u32*)(cdvd.Param + 0);
			cdvd.nSectors = *(u32*)(cdvd.Param + 4);
			cdvd.RetryCnt = (cdvd.Param[8] == 0) ? 0x100 : cdvd.Param[8];
			cdvd.SpindlCtrl = cdvd.Param[9];
			cdvd.Speed = 24;
			switch (cdvd.Param[10])
			{
				case 2:
					cdvd.ReadMode = CDVD_MODE_2340;
					cdvd.BlockSize = 2340;
					break;
				case 1:
					cdvd.ReadMode = CDVD_MODE_2328;
					cdvd.BlockSize = 2328;
					break;
				case 0:
				default:
					cdvd.ReadMode = CDVD_MODE_2048;
					cdvd.BlockSize = 2048;
					break;
			}

			cdvd.ReadTime = cdvdBlockReadTime(MODE_CDROM);
			CDVDREAD_INT(cdvdStartSeek(cdvd.SeekToSector, MODE_CDROM));

			// Read-ahead by telling the plugin about the track now.
			// This helps improve performance on actual from-cd emulation
			// (ie, not using the hard drive)
			cdvd.RErr = DoCDVDreadTrack(cdvd.SeekToSector, cdvd.ReadMode);

			// Set the reading block flag.  If a seek is pending then Readed will
			// take priority in the handler anyway.  If the read is contiguous then
			// this'll skip the seek delay.
			cdvd.Reading = 1;
			break;

		case N_CD_READ_CDDA:  // CdReadCDDA
		case N_CD_READ_XCDDA: // CdReadXCDDA
			// Assign the seek to sector based on cdvd.Param[0]-[3], and the number of  sectors based on cdvd.Param[4]-[7].
			cdvd.SeekToSector = *(u32*)(cdvd.Param + 0);
			cdvd.nSectors = *(u32*)(cdvd.Param + 4);

			if (cdvd.Param[8] == 0)
				cdvd.RetryCnt = 0x100;
			else
				cdvd.RetryCnt = cdvd.Param[8];

			cdvd.SpindlCtrl = cdvd.Param[9];

			switch (cdvd.Param[9])
			{
				case 0x01:
					cdvd.Speed = 1;
					break;
				case 0x02:
					cdvd.Speed = 2;
					break;
				case 0x03:
					cdvd.Speed = 4;
					break;
				case 0x04:
					cdvd.Speed = 12;
					break;
				default:
					cdvd.Speed = 24;
					break;
			}

			switch (cdvd.Param[10])
			{
				case 1:
					cdvd.ReadMode = CDVD_MODE_2368;
					cdvd.BlockSize = 2368;
					break;
				case 2:
				case 0:
					cdvd.ReadMode = CDVD_MODE_2352;
					cdvd.BlockSize = 2352;
					break;
			}

			cdvd.ReadTime = cdvdBlockReadTime(MODE_CDROM);
			CDVDREAD_INT(cdvdStartSeek(cdvd.SeekToSector, MODE_CDROM));

			// Read-ahead by telling the plugin about the track now.
			// This helps improve performance on actual from-cd emulation
			// (ie, not using the hard drive)
			cdvd.RErr = DoCDVDreadTrack(cdvd.SeekToSector, cdvd.ReadMode);

			// Set the reading block flag.  If a seek is pending then Readed will
			// take priority in the handler anyway.  If the read is contiguous then
			// this'll skip the seek delay.
			cdvd.Reading = 1;
			break;

		case N_DVD_READ: // DvdRead
			// Assign the seek to sector based on cdvd.Param[0]-[3], and the number of  sectors based on cdvd.Param[4]-[7].
			cdvd.SeekToSector = *(u32*)(cdvd.Param + 0);
			cdvd.nSectors = *(u32*)(cdvd.Param + 4);

			if (cdvd.Param[8] == 0)
				cdvd.RetryCnt = 0x100;
			else
				cdvd.RetryCnt = cdvd.Param[8];

			cdvd.SpindlCtrl = cdvd.Param[9];
			cdvd.Speed = 4;
			cdvd.ReadMode = CDVD_MODE_2048;
			cdvd.BlockSize = 2064; // Why oh why was it 2064

			cdvd.ReadTime = cdvdBlockReadTime(MODE_DVDROM);
			CDVDREAD_INT(cdvdStartSeek(cdvd.SeekToSector, MODE_DVDROM));

			// Read-ahead by telling the plugin about the track now.
			// This helps improve performance on actual from-cd emulation
			// (ie, not using the hard drive)
			cdvd.RErr = DoCDVDreadTrack(cdvd.SeekToSector, cdvd.ReadMode);

			// Set the reading block flag.  If a seek is pending then Readed will
			// take priority in the handler anyway.  If the read is contiguous then
			// this'll skip the seek delay.
			cdvd.Reading = 1;
			break;

		case N_CD_GET_TOC: // CdGetToc & cdvdman_call19
			//Param[0] is 0 for CdGetToc and any value for cdvdman_call19
			cdvdGetToc(iopPhysMem(HW_DMA3_MADR));
			cdvdSetIrq((1 << Irq_CommandComplete)); //| (1<<Irq_DataReady) );
			HW_DMA3_CHCR &= ~0x01000000;
			psxDmaInterrupt(3);
			break;

		case N_CD_READ_KEY: // CdReadKey
		{
			u8 arg0 = cdvd.Param[0];
			u16 arg1 = cdvd.Param[1] | (cdvd.Param[2] << 8);
			u32 arg2 = cdvd.Param[3] | (cdvd.Param[4] << 8) | (cdvd.Param[5] << 16) | (cdvd.Param[6] << 24);
			cdvdReadKey(arg0, arg1, arg2, cdvd.Key);
			cdvd.KeyXor = 0x00;
			cdvdSetIrq();
		}
		break;

		case N_CD_CHG_SPDL_CTRL: // CdChgSpdlCtrl
			cdvdSetIrq();
			break;

		default:
			cdvdSetIrq();
			break;
	}
	cdvd.ParamP = 0;
	cdvd.ParamC = 0;
}

static __fi void cdvdWrite05(u8 rt)
{ // NDATAIN
	if (cdvd.ParamP < 32)
	{
		cdvd.Param[cdvd.ParamP++] = rt;
		cdvd.ParamC++;
	}
}

static __fi void cdvdWrite07(u8 rt) // BREAK
{
	// If we're already in a Ready state or already Breaking, then do nothing:
	if ((cdvd.Ready != CDVD_NOTREADY) || (cdvd.Action == cdvdAction_Break))
		return;

	// Aborts any one of several CD commands:
	// Pause, Seek, Read, Status, Standby, and Stop

	psxRegs.interrupt &= ~((1 << IopEvt_Cdvd) | (1 << IopEvt_CdvdRead));

	cdvd.Action = cdvdAction_Break;
	CDVD_INT(64);

	// Clear the cdvd status:
	cdvd.Readed = 0;
	cdvd.Reading = 0;
	cdvd.Status = CDVD_STATUS_STOP;
}

static void cdvdWrite16(u8 rt) // SCOMMAND
{
	try
	{
		//	cdvdTN	diskInfo;
		//	cdvdTD	trackInfo;
		//	int i, lbn, type, min, sec, frm, address;
		int address;
		u8 tmp;

		cdvd.sCommand = rt;
		cdvd.Result[0] = 0; // assume success -- failures will overwrite this with an error code.

		switch (rt)
		{
				//		case 0x01: // GetDiscType - from cdvdman (0:1)
				//			SetResultSize(1);
				//			cdvd.Result[0] = 0;
				//			break;

			case 0x02: // CdReadSubQ  (0:11)
				SetResultSize(11);
				cdvd.Result[0] = cdvdReadSubQ(cdvd.Sector, (cdvdSubQ*)&cdvd.Result[1]);
				break;

			case 0x03: // Mecacon-command
				switch (cdvd.Param[0])
				{
					case 0x00: // get mecha version (1:4)
						SetResultSize(4);
						cdvdGetMechaVer(&cdvd.Result[0]);
						break;

					case 0x44: // write console ID (9:1)
						SetResultSize(1);
						cdvdWriteConsoleID(&cdvd.Param[1]);
						break;

					case 0x45: // read console ID (1:9)
						SetResultSize(9);
						cdvdReadConsoleID(&cdvd.Result[1]);
						break;

					case 0xFD: // _sceCdReadRenewalDate (1:6) BCD
						SetResultSize(6);
						cdvd.Result[0] = 0;
						cdvd.Result[1] = 0x04; //year
						cdvd.Result[2] = 0x12; //month
						cdvd.Result[3] = 0x10; //day
						cdvd.Result[4] = 0x01; //hour
						cdvd.Result[5] = 0x30; //min
						break;

					case 0xEF: // read console temperature (1:3)
						// This returns a fixed value of 30.5 C
						SetResultSize(3);
						cdvd.Result[0] = 0; // returns 0 on success
						cdvd.Result[1] = 0x0F; // last 8 bits for integer
						cdvd.Result[2] = 0x05; // leftmost bit for integer, other 7 bits for decimal place
						break;

					default:
						SetResultSize(1);
						cdvd.Result[0] = 0x80;
						break;
				}
				break;

			case 0x05: // CdTrayReqState  (0:1) - resets the tray open detection

				// Fixme: This function is believed to change some status flag
				// when the Tray state (stored as "1" in cdvd.Status) is different between 2 successive calls.
				// Cdvd.Status can be different than 1 here, yet we may still have to report an open status.
				// Gonna have to investigate further. (rama)

				SetResultSize(1);

				if (cdvd.Status == CDVD_STATUS_TRAY_OPEN)
					cdvd.Result[0] = 1;
				else
					cdvd.Result[0] = 0; // old behaviour was always this

				break;

			case 0x06: // CdTrayCtrl  (1:1)
				SetResultSize(1);
				if (cdvd.Param[0] == 0)
					cdvd.Result[0] = cdvdCtrlTrayOpen();
				else
					cdvd.Result[0] = cdvdCtrlTrayClose();
				break;

			case 0x08: // CdReadRTC (0:8)
				SetResultSize(8);
				cdvd.Result[0] = 0;
				cdvd.Result[1] = itob(cdvd.RTC.second); //Seconds
				cdvd.Result[2] = itob(cdvd.RTC.minute); //Minutes
				cdvd.Result[3] = itob(cdvd.RTC.hour);   //Hours
				cdvd.Result[4] = 0;                     //Nothing
				cdvd.Result[5] = itob(cdvd.RTC.day);    //Day
				cdvd.Result[6] = itob(cdvd.RTC.month);  //Month
				cdvd.Result[7] = itob(cdvd.RTC.year);   //Year
				break;

			case 0x09: // sceCdWriteRTC (7:1)
				SetResultSize(1);
				cdvd.Result[0] = 0;
				cdvd.RTC.pad = 0;

				cdvd.RTC.second = btoi(cdvd.Param[cdvd.ParamP - 7]);
				cdvd.RTC.minute = btoi(cdvd.Param[cdvd.ParamP - 6]) % 60;
				cdvd.RTC.hour = btoi(cdvd.Param[cdvd.ParamP - 5]) % 24;
				cdvd.RTC.day = btoi(cdvd.Param[cdvd.ParamP - 3]);
				cdvd.RTC.month = btoi(cdvd.Param[cdvd.ParamP - 2] & 0x7f);
				cdvd.RTC.year = btoi(cdvd.Param[cdvd.ParamP - 1]);
				break;

			case 0x0A: // sceCdReadNVM (2:3)
				address = (cdvd.Param[0] << 8) | cdvd.Param[1];

				if (address < 512)
				{
					SetResultSize(3);
					cdvdReadNVM(&cdvd.Result[1], address * 2, 2);
					// swap bytes around
					tmp = cdvd.Result[1];
					cdvd.Result[1] = cdvd.Result[2];
					cdvd.Result[2] = tmp;
				}
				else
				{
					SetResultSize(1);
					cdvd.Result[0] = 0xff;
				}
				break;

			case 0x0B: // sceCdWriteNVM (4:1)
				SetResultSize(1);
				address = (cdvd.Param[0] << 8) | cdvd.Param[1];

				if (address < 512)
				{
					// swap bytes around
					tmp = cdvd.Param[2];
					cdvd.Param[2] = cdvd.Param[3];
					cdvd.Param[3] = tmp;
					cdvdWriteNVM(&cdvd.Param[2], address * 2, 2);
				}
				else
				{
					cdvd.Result[0] = 0xff;
				}
				break;

				//		case 0x0C: // sceCdSetHDMode (1:1)
				//			break;


			case 0x0F: // sceCdPowerOff (0:1)- Call74 from Xcdvdman
				SetResultSize(1);
				cdvd.Result[0] = 0;
				break;

			case 0x12: // sceCdReadILinkId (0:9)
				SetResultSize(9);
				cdvdReadILinkID(&cdvd.Result[1]);
				break;

			case 0x13: // sceCdWriteILinkID (8:1)
				SetResultSize(1);
				cdvdWriteILinkID(&cdvd.Param[1]);
				break;

			case 0x14: // CdCtrlAudioDigitalOut (1:1)
				//parameter can be 2, 0, ...
				SetResultSize(1);
				cdvd.Result[0] = 0; //8 is a flag; not used
				break;

			case 0x15: // sceCdForbidDVDP (0:1)
				SetResultSize(1);
				cdvd.Result[0] = 5;
				break;

			case 0x16: // AutoAdjustCtrl - from cdvdman (1:1)
				SetResultSize(1);
				cdvd.Result[0] = 0;
				break;

			case 0x17: // CdReadModelNumber (1:9) - from xcdvdman
				SetResultSize(9);
				cdvdReadModelNumber(&cdvd.Result[1], cdvd.Param[0]);
				break;

			case 0x18: // CdWriteModelNumber (9:1) - from xcdvdman
				SetResultSize(1);
				cdvdWriteModelNumber(&cdvd.Param[1], cdvd.Param[0]);
				break;

				//		case 0x19: // sceCdForbidRead (0:1) - from xcdvdman
				//			break;

			case 0x1A:              // sceCdBootCertify (4:1)//(4:16 in psx?)
				SetResultSize(1);   //on input there are 4 bytes: 1;?10;J;C for 18000; 1;60;E;C for 39002 from ROMVER
				cdvd.Result[0] = 1; //i guess that means okay
				break;

			case 0x1B: // sceCdCancelPOffRdy (0:1) - Call73 from Xcdvdman (1:1)
				SetResultSize(1);
				cdvd.Result[0] = 0;
				break;

			case 0x1C: // sceCdBlueLEDCtl (1:1) - Call72 from Xcdvdman
				SetResultSize(1);
				cdvd.Result[0] = 0;
				break;

				//		case 0x1D: // cdvdman_call116 (0:5) - In V10 Bios
				//			break;

			case 0x1E: // sceRemote2Read (0:5) - // 00 14 AA BB CC -> remote key code
				SetResultSize(5);
				cdvd.Result[0] = 0x00;
				cdvd.Result[1] = 0x14;
				cdvd.Result[2] = 0x00;
				cdvd.Result[3] = 0x00;
				cdvd.Result[4] = 0x00;
				break;

				//		case 0x1F: // sceRemote2_7 (2:1) - cdvdman_call117
				//			break;

			case 0x20: // sceRemote2_6 (0:3)	// 00 01 00
				SetResultSize(3);
				cdvd.Result[0] = 0x00;
				cdvd.Result[1] = 0x01;
				cdvd.Result[2] = 0x00;
				break;

				//		case 0x21: // sceCdWriteWakeUpTime (8:1)
				//			break;

			case 0x22: // sceCdReadWakeUpTime (0:10)
				SetResultSize(10);
				cdvd.Result[0] = 0;
				cdvd.Result[1] = 0;
				cdvd.Result[2] = 0;
				cdvd.Result[3] = 0;
				cdvd.Result[4] = 0;
				cdvd.Result[5] = 0;
				cdvd.Result[6] = 0;
				cdvd.Result[7] = 0;
				cdvd.Result[8] = 0;
				cdvd.Result[9] = 0;
				break;

			case 0x24: // sceCdRCBypassCtrl (1:1) - In V10 Bios
				// FIXME: because PRId<0x23, the bit 0 of sio2 don't get updated 0xBF808284
				SetResultSize(1);
				cdvd.Result[0] = 0;
				break;

				//		case 0x25: // cdvdman_call120 (1:1) - In V10 Bios
				//			break;

				//		case 0x26: // cdvdman_call128 (0,3) - In V10 Bios
				//			break;

				//		case 0x27: // cdvdman_call148 (0:13) - In V10 Bios
				//			break;

				//		case 0x28: // cdvdman_call150 (1:1) - In V10 Bios
				//			break;

			case 0x29: //sceCdNoticeGameStart (1:1)
				SetResultSize(1);
				cdvd.Result[0] = 0;
				break;

				//		case 0x2C: //sceCdXBSPowerCtl (2:2)
				//			break;

				//		case 0x2D: //sceCdXLEDCtl (2:2)
				//			break;

				//		case 0x2E: //sceCdBuzzerCtl (0:1)
				//			break;

				//		case 0x2F: //cdvdman_call167 (16:1)
				//			break;

				//		case 0x30: //cdvdman_call169 (1:9)
				//			break;

			case 0x31: //sceCdSetMediumRemoval (1:1)
				SetResultSize(1);
				cdvd.Result[0] = 0;
				break;

			case 0x32: //sceCdGetMediumRemoval (0:2)
				SetResultSize(2);
				cdvd.Result[0] = 0;
				//cdvd.Result[0] = 0; // fixme: I'm pretty sure that the same variable shouldn't be set twice here. Perhaps cdvd.Result[1]?
				break;

				//		case 0x33: //sceCdXDVRPReset (1:1)
				//			break;

			case 0x36: //cdvdman_call189 [__sceCdReadRegionParams - made up name] (0:15) i think it is 16, not 15
				SetResultSize(15);

				cdvdGetMechaVer(&cdvd.Result[1]);
				cdvdReadRegionParams(&cdvd.Result[3]); //size==8
				cdvd.Result[1] = 1 << cdvd.Result[1]; //encryption zone; see offset 0x1C in encrypted headers
				//////////////////////////////////////////
				cdvd.Result[2] = 0; //??
				//			cdvd.Result[3] == ROMVER[4] == *0xBFC7FF04
				//			cdvd.Result[4] == OSDVER[4] == CAP			Jjpn, Aeng, Eeng, Heng, Reng, Csch, Kkor?
				//			cdvd.Result[5] == OSDVER[5] == small
				//			cdvd.Result[6] == OSDVER[6] == small
				//			cdvd.Result[7] == OSDVER[7] == small
				//			cdvd.Result[8] == VERSTR[0x22] == *0xBFC7FF52
				//			cdvd.Result[9] == DVDID						J U O E A R C M
				//			cdvd.Result[10]== 0;					//??
				cdvd.Result[11] = 0; //??
				cdvd.Result[12] = 0; //??
				//////////////////////////////////////////
				cdvd.Result[13] = 0; //0xFF - 77001
				cdvd.Result[14] = 0; //??
				break;

			case 0x37: //called from EECONF [sceCdReadMAC - made up name] (0:9)
				SetResultSize(9);
				cdvdReadMAC(&cdvd.Result[1]);
				break;

			case 0x38: //used to fix the MAC back after accidentally trashed it :D [sceCdWriteMAC - made up name] (8:1)
				SetResultSize(1);
				cdvdWriteMAC(&cdvd.Param[0]);
				break;

			case 0x3E: //[__sceCdWriteRegionParams - made up name] (15:1) [Florin: hum, i was expecting 14:1]
				SetResultSize(1);
				cdvdWriteRegionParams(&cdvd.Param[2]);
				break;

			case 0x40: // CdOpenConfig (3:1)
				SetResultSize(1);
				cdvd.CReadWrite = cdvd.Param[0];
				cdvd.COffset = cdvd.Param[1];
				cdvd.CNumBlocks = cdvd.Param[2];
				cdvd.CBlockIndex = 0;
				cdvd.Result[0] = 0;
				break;

			case 0x41: // CdReadConfig (0:16)
				SetResultSize(16);
				cdvdReadConfig(&cdvd.Result[0]);
				break;

			case 0x42: // CdWriteConfig (16:1)
				SetResultSize(1);
				cdvdWriteConfig(&cdvd.Param[0]);
				break;

			case 0x43: // CdCloseConfig (0:1)
				SetResultSize(1);
				cdvd.CReadWrite = 0;
				cdvd.COffset = 0;
				cdvd.CNumBlocks = 0;
				cdvd.CBlockIndex = 0;
				cdvd.Result[0] = 0;
				break;

			case 0x80:                // secrman: __mechacon_auth_0x80
				SetResultSize(1);     //in:1
				cdvd.mg_datatype = 0; //data
				cdvd.Result[0] = 0;
				break;

			case 0x81:                // secrman: __mechacon_auth_0x81
				SetResultSize(1);     //in:1
				cdvd.mg_datatype = 0; //data
				cdvd.Result[0] = 0;
				break;

			case 0x82:            // secrman: __mechacon_auth_0x82
				SetResultSize(1); //in:16
				cdvd.Result[0] = 0;
				break;

			case 0x83:            // secrman: __mechacon_auth_0x83
				SetResultSize(1); //in:8
				cdvd.Result[0] = 0;
				break;

			case 0x84:                    // secrman: __mechacon_auth_0x84
				SetResultSize(1 + 8 + 4); //in:0
				cdvd.Result[0] = 0;

				cdvd.Result[1] = 0x21;
				cdvd.Result[2] = 0xdc;
				cdvd.Result[3] = 0x31;
				cdvd.Result[4] = 0x96;
				cdvd.Result[5] = 0xce;
				cdvd.Result[6] = 0x72;
				cdvd.Result[7] = 0xe0;
				cdvd.Result[8] = 0xc8;

				cdvd.Result[9] = 0x69;
				cdvd.Result[10] = 0xda;
				cdvd.Result[11] = 0x34;
				cdvd.Result[12] = 0x9b;
				break;

			case 0x85:                    // secrman: __mechacon_auth_0x85
				SetResultSize(1 + 4 + 8); //in:0
				cdvd.Result[0] = 0;

				cdvd.Result[1] = 0xeb;
				cdvd.Result[2] = 0x01;
				cdvd.Result[3] = 0xc7;
				cdvd.Result[4] = 0xa9;

				cdvd.Result[5] = 0x3f;
				cdvd.Result[6] = 0x9c;
				cdvd.Result[7] = 0x5b;
				cdvd.Result[8] = 0x19;
				cdvd.Result[9] = 0x31;
				cdvd.Result[10] = 0xa0;
				cdvd.Result[11] = 0xb3;
				cdvd.Result[12] = 0xa3;
				break;

			case 0x86:            // secrman: __mechacon_auth_0x86
				SetResultSize(1); //in:16
				cdvd.Result[0] = 0;
				break;

			case 0x87:            // secrman: __mechacon_auth_0x87
				SetResultSize(1); //in:8
				cdvd.Result[0] = 0;
				break;

			case 0x8D:            // sceMgWriteData
				SetResultSize(1); //in:length<=16
				if (cdvd.mg_size + cdvd.ParamC > cdvd.mg_maxsize)
				{
					cdvd.Result[0] = 0x80;
				}
				else
				{
					memcpy(cdvd.mg_buffer + cdvd.mg_size, cdvd.Param, cdvd.ParamC);
					cdvd.mg_size += cdvd.ParamC;
					cdvd.Result[0] = 0; // 0 complete ; 1 busy ; 0x80 error
				}
				break;

			case 0x8E: // sceMgReadData
				SetResultSize(std::min(16, cdvd.mg_size));
				memcpy(cdvd.Result, cdvd.mg_buffer, cdvd.ResultC);
				cdvd.mg_size -= cdvd.ResultC;
				memcpy(cdvd.mg_buffer, cdvd.mg_buffer + cdvd.ResultC, cdvd.mg_size);
				break;

			case 0x88:                     // secrman: __mechacon_auth_0x88	//for now it is the same; so, fall;)
			case 0x8F:                     // secrman: __mechacon_auth_0x8F
				SetResultSize(1);          //in:0
				if (cdvd.mg_datatype == 1) // header data
				{
					u64 *psrc, *pdst;
					int bit_ofs, i;

					if ((cdvd.mg_maxsize != cdvd.mg_size) || (cdvd.mg_size < 0x20) || (cdvd.mg_size != *(u16*)&cdvd.mg_buffer[0x14]))
					{
						cdvd.Result[0] = 0x80;
						break;
					}

					std::string zoneStr;
					for (i = 0; i < 8; i++)
					{
						if (cdvd.mg_buffer[0x1C] & (1 << i))
							zoneStr += mg_zones[i];
					}


					bit_ofs = mg_BIToffset(cdvd.mg_buffer);

					psrc = (u64*)&cdvd.mg_buffer[bit_ofs - 0x20];

					pdst = (u64*)cdvd.mg_kbit;
					pdst[0] = psrc[0];
					pdst[1] = psrc[1];
					//memcpy(cdvd.mg_kbit, &cdvd.mg_buffer[bit_ofs-0x20], 0x10);

					pdst = (u64*)cdvd.mg_kcon;
					pdst[0] = psrc[2];
					pdst[1] = psrc[3];
					//memcpy(cdvd.mg_kcon, &cdvd.mg_buffer[bit_ofs-0x10], 0x10);

					if ((cdvd.mg_buffer[bit_ofs + 5] || cdvd.mg_buffer[bit_ofs + 6] || cdvd.mg_buffer[bit_ofs + 7]) ||
						(cdvd.mg_buffer[bit_ofs + 4] * 16 + bit_ofs + 8 + 16 != *(u16*)&cdvd.mg_buffer[0x14]))
					{
						cdvd.Result[0] = 0x80;
						break;
					}
				}
				cdvd.Result[0] = 0; // 0 complete ; 1 busy ; 0x80 error
				break;

			case 0x90:            // sceMgWriteHeaderStart
				SetResultSize(1); //in:5
				cdvd.mg_size = 0;
				cdvd.mg_datatype = 1; //header data

				cdvd.Result[0] = 0; // 0 complete ; 1 busy ; 0x80 error
				break;

			case 0x91: // sceMgReadBITLength
			{
				SetResultSize(3); //in:0
				int bit_ofs = mg_BIToffset(cdvd.mg_buffer);
				memcpy(cdvd.mg_buffer, &cdvd.mg_buffer[bit_ofs], 8 + 16 * cdvd.mg_buffer[bit_ofs + 4]);

				cdvd.mg_maxsize = 0;                       // don't allow any write
				cdvd.mg_size = 8 + 16 * cdvd.mg_buffer[4]; //new offset, i just moved the data

				cdvd.Result[0] = (cdvd.mg_datatype == 1) ? 0 : 0x80; // 0 complete ; 1 busy ; 0x80 error
				cdvd.Result[1] = (cdvd.mg_size >> 0) & 0xFF;
				cdvd.Result[2] = (cdvd.mg_size >> 8) & 0xFF;
				break;
			}
			case 0x92:            // sceMgWriteDatainLength
				SetResultSize(1); //in:2
				cdvd.mg_size = 0;
				cdvd.mg_datatype = 0; //data (encrypted)
				cdvd.mg_maxsize = cdvd.Param[0] | (((int)cdvd.Param[1]) << 8);
				cdvd.Result[0] = 0; // 0 complete ; 1 busy ; 0x80 error
				break;

			case 0x93:            // sceMgWriteDataoutLength
				SetResultSize(1); //in:2
				if (((cdvd.Param[0] | (((int)cdvd.Param[1]) << 8)) == cdvd.mg_size) && (cdvd.mg_datatype == 0))
				{
					cdvd.mg_maxsize = 0; // don't allow any write
					cdvd.Result[0] = 0;  // 0 complete ; 1 busy ; 0x80 error
				}
				else
				{
					cdvd.Result[0] = 0x80;
				}
				break;

			case 0x94:                // sceMgReadKbit - read first half of BIT key
				SetResultSize(1 + 8); //in:0
				cdvd.Result[0] = 0;

				((int*)(cdvd.Result + 1))[0] = ((int*)cdvd.mg_kbit)[0];
				((int*)(cdvd.Result + 1))[1] = ((int*)cdvd.mg_kbit)[1];
				//memcpy(cdvd.Result+1, cdvd.mg_kbit, 8);
				break;

			case 0x95:                // sceMgReadKbit2 - read second half of BIT key
				SetResultSize(1 + 8); //in:0
				cdvd.Result[0] = 0;
				((int*)(cdvd.Result + 1))[0] = ((int*)(cdvd.mg_kbit + 8))[0];
				((int*)(cdvd.Result + 1))[1] = ((int*)(cdvd.mg_kbit + 8))[1];
				//memcpy(cdvd.Result+1, cdvd.mg_kbit+8, 8);
				break;

			case 0x96:                // sceMgReadKcon - read first half of content key
				SetResultSize(1 + 8); //in:0
				cdvd.Result[0] = 0;
				((int*)(cdvd.Result + 1))[0] = ((int*)cdvd.mg_kcon)[0];
				((int*)(cdvd.Result + 1))[1] = ((int*)cdvd.mg_kcon)[1];
				//memcpy(cdvd.Result+1, cdvd.mg_kcon, 8);
				break;

			case 0x97:                // sceMgReadKcon2 - read second half of content key
				SetResultSize(1 + 8); //in:0
				cdvd.Result[0] = 0;
				((int*)(cdvd.Result + 1))[0] = ((int*)(cdvd.mg_kcon + 8))[0];
				((int*)(cdvd.Result + 1))[1] = ((int*)(cdvd.mg_kcon + 8))[1];
				//memcpy(cdvd.Result+1, cdvd.mg_kcon+8, 8);
				break;

			default:
				// fake a 'correct' command
				SetResultSize(1);   //in:0
				cdvd.Result[0] = 0; // 0 complete ; 1 busy ; 0x80 error
				break;
		} // end switch

		cdvd.ParamP = 0;
		cdvd.ParamC = 0;
	}
	catch (Exception::CannotCreateStream&)
	{
		/* Failed to read/write NVM/MEC file. Check your BIOS setup/permission settings. */
	}
}

static __fi void cdvdWrite17(u8 rt)
{ // SDATAIN
	if (cdvd.ParamP < 32)
	{
		cdvd.Param[cdvd.ParamP++] = rt;
		cdvd.ParamC++;
	}
}

void cdvdWrite(u8 key, u8 rt)
{
	switch (key)
	{
		case 0x04:
			cdvdWrite04(rt);
			break;
		case 0x05:
			cdvdWrite05(rt);
			break;
		case 0x06: /*cdvdWrite06 */
			cdvd.HowTo = rt;
			break;
		case 0x07:
			cdvdWrite07(rt);
			break;
		case 0x08: /* cdvdWrite08 - INTR_STAT */
			cdvd.PwOff &= ~rt;
			break;
		case 0x16:
			cdvdWrite16(rt);
			break;
		case 0x17:
			cdvdWrite17(rt);
			break;
		case 0x3A: /* cdvdWrite3A - DEC-SET */
			cdvd.decSet = rt;
			break;
		case 0x0A: /* cdvdWrite0A */
		case 0x0F: /* cdvdWrite0F */
		case 0x14: /* cdvdWrite14 */
		case 0x18: /*cdvdWrite18 - SDATAOUT */
		default:
			break;
	}
}
