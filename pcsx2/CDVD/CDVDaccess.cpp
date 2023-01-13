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

#include <ctype.h>
#include <time.h>
#include <exception>
#include <memory>

#include "IsoFS/IsoFS.h"
#include "IsoFS/IsoFSCDVD.h"
#include "IsoFileFormats.h"

#include "DebugTools/SymbolMap.h"
#include "../Config.h"

CDVD_API* CDVD = NULL;

// ----------------------------------------------------------------------------
// diskTypeCached
// Internal disc type cache, to reduce the overhead of disc type checks, which are
// performed quite liberally by many games (perhaps intended to keep the PS2 DVD
// from spinning down due to idle activity?).
// Cache is set to -1 for init and when the disc is removed/changed, which invokes
// a new DiskTypeCheck.  All subsequent checks use the non-negative value here.
//
static int diskTypeCached = -1;

// used to bridge the gap between the old getBuffer api and the new getBuffer2 api.
static int lastReadSize;
static u32 lastLSN; // needed for block dumping

//////////////////////////////////////////////////////////////////////////////////////////
// Disk Type detection stuff (from cdvdGigaherz)
//
static int CheckDiskTypeFS(int baseType)
{
	IsoFSCDVD isofs;
	IsoDirectory rootdir(isofs);
	try
	{
		IsoFile file(rootdir, L"SYSTEM.CNF;1");

		int size = file.getLength();

		std::unique_ptr<char[]> buffer(new char[file.getLength() + 1]);
		file.read(buffer.get(), size);
		buffer[size] = '\0';

		char* pos = strstr(buffer.get(), "BOOT2");
		if (pos == NULL)
		{
			pos = strstr(buffer.get(), "BOOT");
			if (pos == NULL)
				return CDVD_TYPE_ILLEGAL;
			return CDVD_TYPE_PSCD;
		}

		return (baseType == CDVD_TYPE_DETCTCD) ? CDVD_TYPE_PS2CD : CDVD_TYPE_PS2DVD;
	}
	catch (Exception::FileNotFound&)
	{
	}

	try
	{
		IsoFile file(rootdir, L"PSX.EXE;1");
		return CDVD_TYPE_PSCD;
	}
	catch (Exception::FileNotFound&)
	{
	}

	try
	{
		IsoFile file(rootdir, L"VIDEO_TS/VIDEO_TS.IFO;1");
		return CDVD_TYPE_DVDV;
	}
	catch (Exception::FileNotFound&)
	{
	}

	return CDVD_TYPE_ILLEGAL; // << Only for discs which aren't ps2 at all.
}

static int FindDiskType(int mType)
{
	int dataTracks = 0;
	int audioTracks = 0;
	int iCDType = mType;
	cdvdTN tn;

	CDVD->getTN(&tn);

	if (tn.strack != tn.etrack) // multitrack == CD.
	{
		iCDType = CDVD_TYPE_DETCTCD;
	}
	else if (mType < 0)
	{
		static u8 bleh[CD_FRAMESIZE_RAW];
		cdvdTD td;

		CDVD->getTD(0, &td);
		if (td.lsn > 452849)
		{
			iCDType = CDVD_TYPE_DETCTDVDS;
		}
		else
		{
			if (DoCDVDreadSector(bleh, 16, CDVD_MODE_2048) == 0)
			{
				//const cdVolDesc& volDesc = (cdVolDesc&)bleh;
				//if(volDesc.rootToc.tocSize == 2048)

				//Horrible hack! in CD images position 166 and 171 have block size but not DVD's
				//It's not always 2048 however (can be 4096)
				//Test Impossible Mission if thia is changed.
				if (*(u16*)(bleh + 166) == *(u16*)(bleh + 171))
					iCDType = CDVD_TYPE_DETCTCD;
				else
					iCDType = CDVD_TYPE_DETCTDVDS;
			}
		}
	}

	if (iCDType == CDVD_TYPE_DETCTDVDS)
	{
		s32 dlt = 0;
		u32 l1s = 0;

		if (CDVD->getDualInfo(&dlt, &l1s) == 0)
		{
			if (dlt > 0)
				iCDType = CDVD_TYPE_DETCTDVDD;
		}
	}

	audioTracks = dataTracks = 0;
	for (int i = tn.strack; i <= tn.etrack; i++)
	{
		cdvdTD td, td2;

		CDVD->getTD(i, &td);

		if (tn.etrack > i)
			CDVD->getTD(i + 1, &td2);
		else
			CDVD->getTD(0, &td2);

		if (td.type == CDVD_AUDIO_TRACK)
			audioTracks++;
		else
			dataTracks++;
	}

	if (dataTracks > 0)
	{
		iCDType = CheckDiskTypeFS(iCDType);
	}

	if (audioTracks > 0)
	{
		switch (iCDType)
		{
			case CDVD_TYPE_PS2CD:
				iCDType = CDVD_TYPE_PS2CDDA;
				break;
			case CDVD_TYPE_PSCD:
				iCDType = CDVD_TYPE_PSCDDA;
				break;
			default:
				iCDType = CDVD_TYPE_CDDA;
				break;
		}
	}

	return iCDType;
}

static void DetectDiskType(void)
{
	int baseMediaType = CDVD_TYPE_NODISC;
	if (CDVD->getTrayStatus() != CDVD_TRAY_OPEN)
		baseMediaType = CDVD->getDiskType();

	switch (baseMediaType)
	{
		case CDVD_TYPE_NODISC:
			diskTypeCached = CDVD_TYPE_NODISC;
			break;
		default:
			diskTypeCached = FindDiskType(-1);
			break;
	}
}

static wxString m_SourceFilename[3];
static CDVD_SourceType m_CurrentSourceType = CDVD_SourceType::NoDisc;

void CDVDsys_SetFile(CDVD_SourceType srctype, const wxString& newfile)
{
	m_SourceFilename[enum_cast(srctype)] = newfile;

	// look for symbol file
	if (symbolMap.IsEmpty())
	{
		wxString symName;
		int n = newfile.Last('.');
		if (n == wxNOT_FOUND)
			symName = newfile + L".sym";
		else
			symName = newfile.substr(0, n) + L".sym";

		wxCharBuffer buf = symName.ToUTF8();
		symbolMap.LoadNocashSym(buf);
		symbolMap.UpdateActiveSymbols();
	}
}

const wxString& CDVDsys_GetFile(CDVD_SourceType srctype)
{
	return m_SourceFilename[enum_cast(srctype)];
}

CDVD_SourceType CDVDsys_GetSourceType()
{
	return m_CurrentSourceType;
}

void CDVDsys_ChangeSource(CDVD_SourceType type)
{
	if (CDVD)
		DoCDVDclose();

	switch (m_CurrentSourceType = type)
	{
		case CDVD_SourceType::Iso:
			CDVD = &CDVDapi_Iso;
			break;

		case CDVD_SourceType::Disc:
			CDVD = &CDVDapi_Disc;
			break;

		case CDVD_SourceType::NoDisc:
			CDVD = &CDVDapi_NoDisc;
			break;

		default:
			break;
	}
}

bool DoCDVDopen(void)
{
	CDVD->newDiskCB(cdvdNewDiskCB);

	// Win32 Fail: the old CDVD api expects MBCS on Win32 platforms, but generating a MBCS
	// from unicode is problematic since we need to know the codepage of the text being
	// converted (which isn't really practical knowledge).  A 'best guess' would be the
	// default codepage of the user's Windows install, but even that will fail and return
	// question marks if the filename is another language.

	//TODO_CDVD check if ISO and Disc use UTF8

	auto CurrentSourceType = enum_cast(m_CurrentSourceType);
	int ret = CDVD->open(!m_SourceFilename[CurrentSourceType].IsEmpty() ?
			static_cast<const char*>(m_SourceFilename[CurrentSourceType].ToUTF8()) :
			(char*)NULL);

	if (ret == -1)
		return false;
	DoCDVDdetectDiskType();
	return true;
}

void DoCDVDclose(void)
{
	if (CDVD->close)
		CDVD->close();

	DoCDVDresetDiskTypeCache();
}

s32 DoCDVDreadSector(u8* buffer, u32 lsn, int mode)
{
	return CDVD->readSector(buffer, lsn, mode);
}

s32 DoCDVDreadTrack(u32 lsn, int mode)
{
	// TODO: The CDVD api only uses the new getBuffer style. Why is this temp?
	// lastReadSize is needed for block dumps
	switch (mode)
	{
		case CDVD_MODE_2352:
			lastReadSize = 2352;
			break;
		case CDVD_MODE_2340:
			lastReadSize = 2340;
			break;
		case CDVD_MODE_2328:
			lastReadSize = 2328;
			break;
		case CDVD_MODE_2048:
			lastReadSize = 2048;
			break;
	}

	lastLSN = lsn;
	return CDVD->readTrack(lsn, mode);
}

s32 DoCDVDgetBuffer(u8* buffer)
{
	return CDVD->getBuffer(buffer);
}

s32 DoCDVDdetectDiskType(void)
{
	if (diskTypeCached < 0)
		DetectDiskType();
	return diskTypeCached;
}

void DoCDVDresetDiskTypeCache(void)
{
	diskTypeCached = -1;
}

////////////////////////////////////////////////////////
//
// CDVD null interface for Run BIOS menu

s32 CALLBACK NODISCopen(const char* pTitle)
{
	return 0;
}

void CALLBACK NODISCclose(void)
{
}

s32 CALLBACK NODISCreadTrack(u32 lsn, int mode)
{
	return -1;
}

s32 CALLBACK NODISCgetBuffer(u8* buffer)
{
	return -1;
}

s32 CALLBACK NODISCreadSubQ(u32 lsn, cdvdSubQ* subq)
{
	return -1;
}

s32 CALLBACK NODISCgetTN(cdvdTN* Buffer)
{
	return -1;
}

s32 CALLBACK NODISCgetTD(u8 Track, cdvdTD* Buffer)
{
	return -1;
}

s32 CALLBACK NODISCgetTOC(void* toc)
{
	return -1;
}

s32 CALLBACK NODISCgetDiskType()
{
	return CDVD_TYPE_NODISC;
}

s32 CALLBACK NODISCgetTrayStatus(void)
{
	return CDVD_TRAY_CLOSE;
}

s32 CALLBACK NODISCdummyS32(void)
{
	return 0;
}

void CALLBACK NODISCnewDiskCB(void (* /* callback */)(void))
{
}

s32 CALLBACK NODISCreadSector(u8* tempbuffer, u32 lsn, int mode)
{
	return -1;
}

s32 CALLBACK NODISCgetDualInfo(s32* dualType, u32* _layer1start)
{
	return -1;
}

CDVD_API CDVDapi_NoDisc =
	{
		NODISCclose,
		NODISCopen,
		NODISCreadTrack,
		NODISCgetBuffer,
		NODISCreadSubQ,
		NODISCgetTN,
		NODISCgetTD,
		NODISCgetTOC,
		NODISCgetDiskType,
		NODISCgetTrayStatus,
		NODISCdummyS32,
		NODISCdummyS32,

		NODISCnewDiskCB,

		NODISCreadSector,
		NODISCgetDualInfo,
};
