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

#include <wx/ffile.h>
#include <map>

#include "Utilities/SafeArray.inl"
#include "Utilities/MemcpyFast.h"

#include "MemoryCardFile.h"

#include "../System.h"
#include "AppConfig.h"

#include "svnrev.h"

#include  "options_tools.h"

static const int MCD_SIZE = 1024 * 8 * 16; // Legacy PSX card default size

static const int MC2_MBSIZE = 1024 * 528 * 2; // Size of a single megabyte of card data

// ECC code ported from mymc
// https://sourceforge.net/p/mymc-opl/code/ci/master/tree/ps2mc_ecc.py
// Public domain license

static u32 CalculateECC(u8* buf)
{
	const u8 parity_table[256] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,
	0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,
	1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,
	0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,
	1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,
	1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,
	1,0,1,1,0};

	const u8 column_parity_mask[256] = {0,7,22,17,37,34,51,52,52,51,34,37,17,22,
	7,0,67,68,85,82,102,97,112,119,119,112,97,102,82,85,68,67,82,85,68,67,119,112,
	97,102,102,97,112,119,67,68,85,82,17,22,7,0,52,51,34,37,37,34,51,52,0,7,22,17,
	97,102,119,112,68,67,82,85,85,82,67,68,112,119,102,97,34,37,52,51,7,0,17,22,
	22,17,0,7,51,52,37,34,51,52,37,34,22,17,0,7,7,0,17,22,34,37,52,51,112,119,102,
	97,85,82,67,68,68,67,82,85,97,102,119,112,112,119,102,97,85,82,67,68,68,67,82,
	85,97,102,119,112,51,52,37,34,22,17,0,7,7,0,17,22,34,37,52,51,34,37,52,51,7,0,
	17,22,22,17,0,7,51,52,37,34,97,102,119,112,68,67,82,85,85,82,67,68,112,119,102,
	97,17,22,7,0,52,51,34,37,37,34,51,52,0,7,22,17,82,85,68,67,119,112,97,102,102,
	97,112,119,67,68,85,82,67,68,85,82,102,97,112,119,119,112,97,102,82,85,68,67,
	0,7,22,17,37,34,51,52,52,51,34,37,17,22,7,0};

	u8 column_parity = 0x77;
	u8 line_parity_0 = 0x7F;
	u8 line_parity_1 = 0x7F;

	for (int i = 0; i < 128; i++)
	{
		u8 b = buf[i];
		column_parity ^= column_parity_mask[b];
		if (parity_table[b])
		{
			line_parity_0 ^= ~i;
			line_parity_1 ^= i;
		}
	}

	return column_parity | (line_parity_0 << 8) | (line_parity_1 << 16);
}

static bool ConvertNoECCtoRAW(wxString file_in, wxString file_out)
{
	bool result = false;
	wxFFile fin(file_in, "rb");

	if (fin.IsOpened())
	{
		wxFFile fout(file_out, "wb");

		if (fout.IsOpened())
		{
			u8 buffer[512];
			size_t size = fin.Length();

			for (size_t i = 0; i < (size / 512); i++)
			{
				fin.Read(buffer, 512);
				fout.Write(buffer, 512);

				for (int j = 0; j < 4; j++)
				{
					u32 checksum = CalculateECC(&buffer[j * 128]);
					fout.Write(&checksum, 3);
				}

				fout.Write("\0\0\0\0", 4);
			}

			result = true;
		}
	}

	return result;
}

static bool ConvertRAWtoNoECC(wxString file_in, wxString file_out)
{
	bool result = false;
	wxFFile fout(file_out, "wb");

	if (fout.IsOpened())
	{
		wxFFile fin(file_in, "rb");

		if (fin.IsOpened())
		{
			u8 buffer[512];
			size_t size = fin.Length();

			for (size_t i = 0; i < (size / 528); i++)
			{
				fin.Read(buffer, 512);
				fout.Write(buffer, 512);
				fin.Read(buffer, 16);
			}

			result = true;
		}
	}

	return result;
}

// --------------------------------------------------------------------------------------
//  FileMemoryCard
// --------------------------------------------------------------------------------------
// Provides thread-safe direct file IO mapping.
//
class FileMemoryCard
{
protected:
	wxFFile m_file[8];
	u8 m_effeffs[528 * 16];
	SafeArray<u8> m_currentdata;
	u64 m_chksum[8];
	bool m_ispsx[8];
	u32 m_chkaddr;

public:
	FileMemoryCard();
	virtual ~FileMemoryCard() = default;

	void Open();
	void Close();

	s32 IsPresent(uint slot);
	void GetSizeInfo(uint slot, McdSizeInfo& outways);
	bool IsPSX(uint slot);
	s32 Read(uint slot, u8* dest, u32 adr, int size);
	s32 Save(uint slot, const u8* src, u32 adr, int size);
	s32 EraseBlock(uint slot, u32 adr);
	u64 GetCRC(uint slot);

protected:
	bool Seek(wxFFile& f, u32 adr);
	bool Create(const wxString& mcdFile, uint sizeInMB);

	wxString GetDisabledMessage(uint slot) const
	{
		return wxsFormat(L"The PS2-slot %d has been automatically disabled.  You can correct the problem\nand re-enable it at any time using Config:Memory cards from the main menu.", slot //TODO: translate internal slot index to human-readable slot description
		);
	}
};

static uint FileMcd_GetMtapPort(uint slot)
{
	switch (slot)
	{
		case 1:
		case 5:
		case 6:
		case 7:
			return 1;
		case 0:
		case 2:
		case 3:
		case 4:
		default:
			break;
	}

	return 0;
}

// Returns the multitap slot number, range 1 to 3 
// (slot 0 refers to the standard 1st and 2nd player slots).
static uint FileMcd_GetMtapSlot(uint slot)
{
	switch (slot)
	{
		case 2:
		case 3:
		case 4:
			return slot - 1;
		case 5:
		case 6:
		case 7:
			return slot - 4;
		case 0:
		case 1:
		default:
			break;
	}

	return 0; // technically unreachable.
}

bool FileMcd_IsMultitapSlot(uint slot)
{
	return (slot > 1);
}

wxString FileMcd_GetDefaultName(uint slot)
{
	if (FileMcd_IsMultitapSlot(slot))
		return wxsFormat(L"Mcd-Multitap%u-Slot%02u.ps2", FileMcd_GetMtapPort(slot) + 1, FileMcd_GetMtapSlot(slot) + 1);
	return wxsFormat(L"Mcd%03u.ps2", slot + 1);
}

FileMemoryCard::FileMemoryCard()
{
	memset8<0xff>(m_effeffs);
	m_chkaddr = 0;
}

void FileMemoryCard::Open()
{
	for (int slot = 0; slot < 8; ++slot)
	{

		if (FileMcd_IsMultitapSlot(slot))
		{
			if (!EmuConfig.MultitapPort0_Enabled && (FileMcd_GetMtapPort(slot) == 0))
				continue;
			if (!EmuConfig.MultitapPort1_Enabled && (FileMcd_GetMtapPort(slot) == 1))
				continue;
		}

		wxFileName fname(g_Conf->FullpathToMcd(slot));
		wxString str(fname.GetFullPath());
		bool cont = false;

		if (fname.GetFullName().IsEmpty())
		{
			str = L"[empty filename]";
			cont = true;
		}

		if (!g_Conf->Mcd[slot].Enabled)
		{
			str = L"[disabled]";
			cont = true;
		}

		if (g_Conf->Mcd[slot].Type != MemoryCardType::MemoryCard_File)
		{
			str = L"[is not memcard file]";
			cont = true;
		}

		if (cont)
			continue;

		const wxULongLong fsz = fname.GetSize();
		if ((fsz == 0) || (fsz == wxInvalidSize))
		{
			// FIXME : Ideally this should prompt the user for the size of the
			// memory card file they would like to create, instead of trying to
			// create one automatically.

			if (!Create(str, 8))
			{
				log_cb(RETRO_LOG_ERROR,
					wxsFormat("Could not create a memory card: \n\n%s\n\n %s\n", str.c_str(), GetDisabledMessage(slot).c_str()).c_str());
			}
		}

		// [TODO] : Add memcard size detection and report it to the console log.
		//   (8MB, 256Mb, formatted, unformatted, etc ...)

#ifdef __WXMSW__
		NTFS_CompressFile(str, g_Conf->McdCompressNTFS);
#endif

		if (str.EndsWith(".bin"))
		{
			wxString newname = str + "x";
			if (!ConvertNoECCtoRAW(str, newname))
			{
				log_cb(RETRO_LOG_ERROR, "Could convert memory card: %s", WX_STR(str));
				wxRemoveFile(newname);
				continue;
			}
			str = newname;
		}

		if (!m_file[slot].Open(str.c_str(), L"r+b"))
		{
			// Translation note: detailed description should mention that the memory card will be disabled
			// for the duration of this session.
			log_cb(RETRO_LOG_ERROR,
					wxsFormat("Access denied to memory card: \n\n%s\n\n %s\n", str.c_str(), GetDisabledMessage(slot).c_str()).c_str()
			      );
		}
		else // Load checksum
		{
			m_ispsx[slot] = m_file[slot].Length() == 0x20000;
			m_chkaddr = 0x210;

			if (!m_ispsx[slot] && !!m_file[slot].Seek(m_chkaddr))
				m_file[slot].Read(&m_chksum[slot], 8);
		}
	}
}

void FileMemoryCard::Close()
{
	for (int slot = 0; slot < 8; ++slot)
	{
		if (m_file[slot].IsOpened())
		{
			// Store checksum
			if (!m_ispsx[slot] && !!m_file[slot].Seek(m_chkaddr))
				m_file[slot].Write(&m_chksum[slot], 8);

			m_file[slot].Close();

			if (m_file[slot].GetName().EndsWith(".binx"))
			{
				wxString name = m_file[slot].GetName();
				wxString name_old = name.SubString(0, name.Last('.')) + "bin";
				if (ConvertRAWtoNoECC(name, name_old))
					wxRemoveFile(name);
			}
		}
	}
}

// Returns FALSE if the seek failed (is outside the bounds of the file).
bool FileMemoryCard::Seek(wxFFile& f, u32 adr)
{
	const u32 size = f.Length();

	// If anyone knows why this filesize logic is here (it appears to be related to legacy PSX
	// cards, perhaps hacked support for some special emulator-specific memcard formats that
	// had header info?), then please replace this comment with something useful.  Thanks!  -- air

	u32 offset = 0;

	if (size == MCD_SIZE + 64)
		offset = 64;
	else if (size == MCD_SIZE + 3904)
		offset = 3904;

	return f.Seek(adr + offset);
}

// returns FALSE if an error occurred (either permission denied or disk full)
bool FileMemoryCard::Create(const wxString& mcdFile, uint sizeInMB)
{
	log_cb(RETRO_LOG_INFO, "(FileMcd) Creating new %uMB memory card: %s\n", sizeInMB, WX_STR(mcdFile));

	wxFFile fp(mcdFile, L"wb");
	if (!fp.IsOpened())
		return false;

	for (uint i = 0; i < (MC2_MBSIZE * sizeInMB) / sizeof(m_effeffs); i++)
	{
		if (fp.Write(m_effeffs, sizeof(m_effeffs)) == 0)
			return false;
	}
	return true;
}

s32 FileMemoryCard::IsPresent(uint slot)
{
	return m_file[slot].IsOpened();
}

void FileMemoryCard::GetSizeInfo(uint slot, McdSizeInfo& outways)
{
	outways.SectorSize = 512;             // 0x0200
	outways.EraseBlockSizeInSectors = 16; // 0x0010
	outways.Xor = 18;                     // 0x12, XOR 02 00 00 10

	if (m_file[slot].IsOpened())
		outways.McdSizeInSectors = m_file[slot].Length() / (outways.SectorSize + outways.EraseBlockSizeInSectors);
	else
		outways.McdSizeInSectors = 0x4000;

	u8* pdata = (u8*)&outways.McdSizeInSectors;
	outways.Xor ^= pdata[0] ^ pdata[1] ^ pdata[2] ^ pdata[3];
}

bool FileMemoryCard::IsPSX(uint slot)
{
	return m_ispsx[slot];
}

s32 FileMemoryCard::Read(uint slot, u8* dest, u32 adr, int size)
{
	wxFFile& mcfp(m_file[slot]);
	if (!mcfp.IsOpened()) /* Ignoring attempted read from disabled slot */
	{
		memset(dest, 0, size);
		return 1;
	}
	if (!Seek(mcfp, adr))
		return 0;
	return mcfp.Read(dest, size) != 0;
}

s32 FileMemoryCard::Save(uint slot, const u8* src, u32 adr, int size)
{
	wxFFile& mcfp(m_file[slot]);

	if (!mcfp.IsOpened()) /* Ignoring attempted save/write to disabled slot */
		return 1;

	if (m_ispsx[slot])
	{
		m_currentdata.MakeRoomFor(size);
		for (int i = 0; i < size; i++)
			m_currentdata[i] = src[i];
	}
	else
	{
		if (!Seek(mcfp, adr))
			return 0;
		m_currentdata.MakeRoomFor(size);
		mcfp.Read(m_currentdata.GetPtr(), size);


		for (int i = 0; i < size; i++)
			m_currentdata[i] &= src[i];

		// Checksumness
		{
			u64* pdata = (u64*)&m_currentdata[0];
			u32 loops = size / 8;

			for (u32 i = 0; i < loops; i++)
				m_chksum[slot] ^= pdata[i];
		}
	}

	if (!Seek(mcfp, adr))
		return 0;

	if (mcfp.Write(m_currentdata.GetPtr(), size))
		return 1;

	return 0;
}

s32 FileMemoryCard::EraseBlock(uint slot, u32 adr)
{
	wxFFile& mcfp(m_file[slot]);

	if (!mcfp.IsOpened()) /* Ignoring erase for disabled slot */
		return 1;

	if (!Seek(mcfp, adr))
		return 0;
	return mcfp.Write(m_effeffs, sizeof(m_effeffs)) != 0;
}

u64 FileMemoryCard::GetCRC(uint slot)
{
	wxFFile& mcfp(m_file[slot]);
	if (!mcfp.IsOpened())
		return 0;

	if (m_ispsx[slot])
	{
		u64 retval = 0;
		if (!Seek(mcfp, 0))
			return 0;

		// Process the file in 4k chunks.  Speeds things up significantly.

		u64 buffer[528 * 8]; // use 528 (sector size), ensures even divisibility

		const uint filesize = mcfp.Length() / sizeof(buffer);
		for (uint i = filesize; i; --i)
		{
			mcfp.Read(&buffer, sizeof(buffer));
			for (uint t = 0; t < ARRAY_SIZE(buffer); ++t)
				retval ^= buffer[t];
		}
		return retval;
	}

	return m_chksum[slot];
}

// --------------------------------------------------------------------------------------
//  MemoryCard Component API Bindings
// --------------------------------------------------------------------------------------

namespace Mcd
{
	FileMemoryCard impl;       // class-based implementations we refer to when API is invoked
}; // namespace Mcd

static uint FileMcd_ConvertToSlot(uint port, uint slot)
{
	if (slot == 0)
		return port;
	if (port == 0)
		return slot + 1; // multitap 1
	return slot + 4;     // multitap 2
}

void FileMcd_EmuOpen(void)
{
	Mcd::impl.Open();
}

void FileMcd_EmuClose(void)
{
	Mcd::impl.Close();
}

s32 FileMcd_IsPresent(uint port, uint slot)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	return Mcd::impl.IsPresent(combinedSlot);
}

void FileMcd_GetSizeInfo(uint port, uint slot, McdSizeInfo* outways)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (g_Conf->Mcd[combinedSlot].Type)
	{
		case MemoryCardType::MemoryCard_File:
			Mcd::impl.GetSizeInfo(combinedSlot, *outways);
			break;
		default:
			return;
	}
}

bool FileMcd_IsPSX(uint port, uint slot)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (g_Conf->Mcd[combinedSlot].Type)
	{
		case MemoryCardType::MemoryCard_File:
			return Mcd::impl.IsPSX(combinedSlot);
		default:
                        break;
	}

	return false;
}

s32 FileMcd_Read(uint port, uint slot, u8* dest, u32 adr, int size)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (g_Conf->Mcd[combinedSlot].Type)
	{
		case MemoryCardType::MemoryCard_File:
			return Mcd::impl.Read(combinedSlot, dest, adr, size);
		default:
			break;
	}

	return 0;
}

s32 FileMcd_Save(uint port, uint slot, const u8* src, u32 adr, int size)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (g_Conf->Mcd[combinedSlot].Type)
	{
		case MemoryCardType::MemoryCard_File:
			return Mcd::impl.Save(combinedSlot, src, adr, size);
		default:
			break;
	}

	return 0;
}

s32 FileMcd_EraseBlock(uint port, uint slot, u32 adr)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (g_Conf->Mcd[combinedSlot].Type)
	{
		case MemoryCardType::MemoryCard_File:
			return Mcd::impl.EraseBlock(combinedSlot, adr);
		default:
			break;
	}

	return 0;
}

u64 FileMcd_GetCRC(uint port, uint slot)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (g_Conf->Mcd[combinedSlot].Type)
	{
		case MemoryCardType::MemoryCard_File:
			return Mcd::impl.GetCRC(combinedSlot);
		default:
                        break;
	}

	return 0;
}

void FileMcd_NextFrame(uint port, uint slot)
{
	FileMcd_ConvertToSlot(port, slot);
}

bool FileMcd_ReIndex(uint port, uint slot, const wxString& filter)
{
	FileMcd_ConvertToSlot(port, slot);
	return false;
}
