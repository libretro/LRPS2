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
#include <algorithm>
#include "IsoFileFormats.h"
#include "../Config.h"

int InputIsoFile::ReadSync(u8* dst, uint lsn)
{
	if (lsn >= m_blocks)
		return -1;
	return m_reader->ReadSync(dst + m_blockofs, lsn, 1);
}

void InputIsoFile::BeginRead2(uint lsn)
{
	m_current_lsn = lsn;

	// While this usually indicates that the ISO is corrupted, 
	// some games do attempt
	// to read past the end of the disc, so don't error here.
	if (lsn >= m_blocks)
		return;

	// Already buffered
	if (lsn >= m_read_lsn && lsn < (m_read_lsn + m_read_count))
		return;

	m_read_lsn = lsn;
	m_read_count = 1;

	if (ReadUnit > 1)
		m_read_count = std::min(ReadUnit, m_blocks - m_read_lsn);

	m_reader->BeginRead(m_readbuffer, m_read_lsn, m_read_count);
	m_read_inprogress = true;
}

int InputIsoFile::FinishRead3(u8* dst, uint mode)
{
	// Do nothing for out of bounds disc sector reads. It prevents some games
	// from hanging (All-Star Baseball 2005, Hello Kitty: Roller Rescue,
	// Hot Wheels: Beat That! (NTSC), Ratchet & Clank 3 (PAL),
	// Test Drive: Eve of Destruction, etc.).
	if (m_current_lsn >= m_blocks)
		return 0;

	int _offset = 0;
	int length = 0;
	int ret = 0;

	if (m_read_inprogress)
	{
		ret = m_reader->FinishRead();
		m_read_inprogress = false;

		if (ret < 0)
			return ret;
	}

	switch (mode)
	{
		case CDVD_MODE_2352:
			_offset = 0;
			length = 2352;
			break;
		case CDVD_MODE_2340:
			_offset = 12;
			length = 2340;
			break;
		case CDVD_MODE_2328:
			_offset = 24;
			length = 2328;
			break;
		case CDVD_MODE_2048:
			_offset = 24;
			length = 2048;
			break;
	}

	int end1 = m_blockofs + m_blocksize;
	int end2 = _offset + length;
	int end = std::min(end1, end2);

	int diff = m_blockofs - _offset;
	int ndiff = 0;
	if (diff > 0)
	{
		memset(dst, 0, diff);
		_offset = m_blockofs;
	}
	else
	{
		ndiff = -diff;
		diff = 0;
	}

	length = end - _offset;

	uint read_offset = (m_current_lsn - m_read_lsn) * m_blocksize;
	memcpy(dst + diff, m_readbuffer + ndiff + read_offset, length);

	if (m_type == ISOTYPE_CD && diff >= 12)
	{
		lsn_to_msf(dst + diff - 12, m_current_lsn);
		dst[diff - 9] = 2;
	}

	return 0;
}

InputIsoFile::InputIsoFile()
{
	_init();
}

InputIsoFile::~InputIsoFile()
{
	Close();
}

void InputIsoFile::_init()
{
	m_type = ISOTYPE_ILLEGAL;
	m_flags = 0;

	m_offset = 0;
	m_blockofs = 0;
	m_blocksize = 0;
	m_blocks = 0;

	m_read_inprogress = false;
	m_read_count = 0;
	ReadUnit = 0;
	m_current_lsn = -1;
	m_read_lsn = -1;
	m_reader = NULL;
}

bool InputIsoFile::Open(const wxString& srcfile, bool testOnly)
{
	Close();
	m_filename = srcfile;
	m_reader = NULL;

	bool isBlockdump = false;
	bool isCompressed = false;

	// First try using a compressed reader.  If it works, go with it.
	m_reader = CompressedFileReader::GetNewReader(m_filename);
	isCompressed = m_reader != NULL;

	// If it wasn't compressed, let's open it has a FlatFileReader.
	if (!isCompressed)
	{
		// Allow write sharing of the iso based on the ini settings.
		// Mostly useful for romhacking, where the disc is frequently
		// changed and the emulator would block modifications
		m_reader = new FlatFileReader(EmuConfig.CdvdShareWrite);
	}

	m_reader->Open(m_filename);

	// It might actually be a blockdump file.
	// Check that before continuing with the FlatFileReader.
	isBlockdump = BlockdumpFileReader::DetectBlockdump(m_reader);
	if (isBlockdump)
	{
		delete m_reader;

		BlockdumpFileReader* bdr = new BlockdumpFileReader();
		bdr->Open(m_filename);

		m_blockofs = bdr->GetBlockOffset();
		m_blocksize = bdr->GetBlockSize();

		m_reader = bdr;

		ReadUnit = 1;
	}

	bool detected = Detect();

	if (testOnly)
		return detected;

	if (!detected)
		throw Exception::BadStream()
			.SetUserMsg(L"Unrecognized ISO image file format")
			.SetDiagMsg(L"ISO mounting failed: PCSX2 is unable to identify the ISO image type.");

	if (!isBlockdump && !isCompressed)
	{
		ReadUnit = MaxReadUnit;

		m_reader->SetDataOffset(m_offset);
		m_reader->SetBlockSize(m_blocksize);

		// Returns the original reader if single-part or a Multipart reader otherwise
		AsyncFileReader* m_reader_old = m_reader;
		m_reader = MultipartFileReader::DetectMultipart(m_reader);
		if (m_reader != m_reader_old) // Not the same object the old one need to be deleted
			delete m_reader_old;
	}

	m_blocks = m_reader->GetBlockCount();

	return true;
}

void InputIsoFile::Close()
{
	delete m_reader;
	m_reader = NULL;

	_init();
}

bool InputIsoFile::IsOpened() const
{
	return m_reader != NULL;
}

bool InputIsoFile::tryIsoType(u32 _size, s32 _offset, s32 _blockofs)
{
	static u8 buf[2456];

	m_blocksize = _size;
	m_offset = _offset;
	m_blockofs = _blockofs;

	m_reader->SetDataOffset(_offset);
	m_reader->SetBlockSize(_size);

	if (ReadSync(buf, 16) < 0)
		return false;

	if (strncmp((char*)(buf + 25), "CD001", 5)) // Not ISO 9660 compliant
		return false;

	m_type = (*(u16*)(buf + 190) == 2048) ? ISOTYPE_CD : ISOTYPE_DVD;

	return true; // We can deal with this.
}

// based on florin's CDVDbin detection code :)
// Parameter:
//
//
// Returns true if the image is valid/known/supported, or false if not (type == ISOTYPE_ILLEGAL).
bool InputIsoFile::Detect(bool readType)
{
	m_type = ISOTYPE_ILLEGAL;

	AsyncFileReader* headpart = m_reader;

	// First sanity check: no sane CD image has less than 16 sectors, since that's what
	// we need simply to contain a TOC.  So if the file size is not large enough to
	// accommodate that, it is NOT a CD image --->

	int sectors = headpart->GetBlockCount();

	if (sectors < 17)
		return false;

	m_blocks = 17;

	if (tryIsoType(2048, 0, 24))
		return true; // ISO 2048
	if (tryIsoType(2336, 0, 16))
		return true; // RAW 2336
	if (tryIsoType(2352, 0, 0))
		return true; // RAW 2352
	if (tryIsoType(2448, 0, 0))
		return true; // RAWQ 2448

	if (tryIsoType(2048, 150 * 2048, 24))
		return true; // NERO ISO 2048
	if (tryIsoType(2352, 150 * 2048, 0))
		return true; // NERO RAW 2352
	if (tryIsoType(2448, 150 * 2048, 0))
		return true; // NERO RAWQ 2448

	if (tryIsoType(2048, -8, 24))
		return true; // ISO 2048
	if (tryIsoType(2352, -8, 0))
		return true; // RAW 2352
	if (tryIsoType(2448, -8, 0))
		return true; // RAWQ 2448

	m_offset = 0;
	m_blocksize = CD_FRAMESIZE_RAW;
	m_blockofs = 0;
	m_type = ISOTYPE_AUDIO;

	m_reader->SetDataOffset(m_offset);
	m_reader->SetBlockSize(m_blocksize);

	//BUG: This also detects a memory-card-file as a valid Audio-CD ISO... -avih
	return true;
}
