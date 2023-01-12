/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#include <wx/wfstream.h>

#include "IsoFileFormats.h"

enum isoFlags
{
	ISOFLAGS_BLOCKDUMP_V2 = 0x0004,
	ISOFLAGS_BLOCKDUMP_V3 = 0x0020
};

static const uint BlockDumpHeaderSize = 16;

bool BlockdumpFileReader::DetectBlockdump(AsyncFileReader* reader)
{
	uint oldbs = reader->GetBlockSize();

	reader->SetBlockSize(1);

	char buf[5] = {0};
	reader->ReadSync(buf, 0, 4);

	bool isbd = (strncmp(buf, "BDV2", 4) == 0);

	if (!isbd)
		reader->SetBlockSize(oldbs);

	return isbd;
}

BlockdumpFileReader::BlockdumpFileReader(void)
	: m_file(NULL)
	, m_blocks(0)
	, m_blockofs(0)
	, m_dtablesize(0)
	, m_lresult(0)
{
}

BlockdumpFileReader::~BlockdumpFileReader(void)
{
	Close();
}

bool BlockdumpFileReader::Open(const wxString& fileName)
{
	char buf[32];

	m_filename = fileName;

	m_file = new wxFileInputStream(m_filename);

	m_file->SeekI(0);
	m_file->Read(buf, 4);

	if (strncmp(buf, "BDV2", 4) != 0)
		return false;

	m_file->Read(&m_blocksize, sizeof(m_blocksize));
	m_file->Read(&m_blocks, sizeof(m_blocks));
	m_file->Read(&m_blockofs, sizeof(m_blockofs));

	wxFileOffset flen = m_file->GetLength();
	static const wxFileOffset datalen = flen - BlockDumpHeaderSize;

	m_dtablesize = datalen / (m_blocksize + 4);
	m_dtable = std::unique_ptr<u32[]>(new u32[m_dtablesize]);

	m_file->SeekI(BlockDumpHeaderSize);

	u32 bs = 1024 * 1024;
	u32 off = 0;
	u32 has = 0;
	int i = 0;

	std::unique_ptr<u8[]> buffer(new u8[bs]);
	do
	{
		m_file->Read(buffer.get(), bs);
		has = m_file->LastRead();

		while (i < m_dtablesize && off < has)
		{
			m_dtable[i++] = *reinterpret_cast<u32*>(buffer.get() + off);
			off += 4;
			off += m_blocksize;
		}

		off -= has;

	} while (has == bs);

	return true;
}

int BlockdumpFileReader::ReadSync(void* pBuffer, uint lsn, uint count)
{
	u8* dst = (u8*)pBuffer;

	while (count > 0)
	{
		bool ok = false;
		for (int i = 0; i < m_dtablesize; ++i)
		{
			if (m_dtable[i] != lsn)
				continue;

			// We store the LSN (u32) along with each block inside of blockdumps, so the
			// seek position ends up being based on (m_blocksize + 4) instead of just m_blocksize.
			m_file->SeekI(BlockDumpHeaderSize + (i * (m_blocksize + 4)) + 4);
			m_file->Read(dst, m_blocksize);

			ok = true;
			break;
		}

		if (!ok)
			return -1;

		count--;
		lsn++;
		dst += m_blocksize;
	}

	return 0;
}

void BlockdumpFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	m_lresult = ReadSync(pBuffer, sector, count);
}

int BlockdumpFileReader::FinishRead(void)
{
	return m_lresult;
}

void BlockdumpFileReader::CancelRead(void)
{
}

void BlockdumpFileReader::Close(void)
{
	if (m_file)
	{
		delete m_file;
		m_file = NULL;
	}
}

uint BlockdumpFileReader::GetBlockCount(void) const
{
	return m_blocks;
}
