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

#include "PrecompiledHeader.h"
#include "AsyncFileReader.h"

#if defined(__APPLE__)
#warning Tested on FreeBSD, not OS X. Be very afraid.
#endif

// The aio module has been reported to cause issues with FreeBSD 10.3, so let's
// disable it for 10.3 and earlier and hope FreeBSD 11 and onwards is fine.
// Note: It may be worth checking whether aio provides any performance benefit.
#if defined(__FreeBSD__) && __FreeBSD__ < 11
#define DISABLE_AIO
#warning AIO has been disabled.
#endif

FlatFileReader::FlatFileReader(bool shareWrite) : shareWrite(shareWrite)
{
	m_blocksize = 2048;
	m_fd = -1;
	m_read_in_progress = false;
}

FlatFileReader::~FlatFileReader(void)
{
	Close();
}

bool FlatFileReader::Open(const wxString& fileName)
{
    m_filename = fileName;

    m_fd = wxOpen(fileName, O_RDONLY, 0);

	return (m_fd != -1);
}

int FlatFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	BeginRead(pBuffer, sector, count);
	return FinishRead();
}

void FlatFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	u64 offset = sector * (u64)m_blocksize + m_dataoffset;

	u32 bytesToRead = count * m_blocksize;

#if defined(DISABLE_AIO)
	m_aiocb.aio_nbytes = pread(m_fd, pBuffer, bytesToRead, offset);
	if (m_aiocb.aio_nbytes != bytesToRead)
		m_aiocb.aio_nbytes = -1;
#else
	m_aiocb = {0};
	m_aiocb.aio_fildes = m_fd;
	m_aiocb.aio_offset = offset;
	m_aiocb.aio_nbytes = bytesToRead;
	m_aiocb.aio_buf = pBuffer;

	if (aio_read(&m_aiocb) != 0) {
#if defined(__FreeBSD__)
		if (errno == ENOSYS)
			log_cb(RETRO_LOG_ERROR, "AIO read failed: Check the aio kernel module is loaded\n");
		else
			log_cb(RETRO_LOG_ERROR, "AIO read failed: error code %d\n", errno);
#else
		log_cb(RETRO_LOG_ERROR, "AIO read failed: error code %d\n", errno);
#endif
		return;
	}
#endif
	m_read_in_progress = true;
}

int FlatFileReader::FinishRead(void)
{
#if defined(DISABLE_AIO)
	m_read_in_progress = false;
	return m_aiocb.aio_nbytes == (size_t)-1 ? -1: 1;
#else
	struct aiocb *aiocb_list[] = {&m_aiocb};

	while (aio_suspend(aiocb_list, 1, nullptr) == -1)
		if (errno != EINTR)
			break;

	m_read_in_progress = false;
	return aio_return(&m_aiocb) == -1? -1: 1;
#endif
}

void FlatFileReader::CancelRead(void)
{
#if !defined(DISABLE_AIO)
	aio_cancel(m_fd, &m_aiocb);
#endif
	m_read_in_progress = false;
}

void FlatFileReader::Close(void)
{
	if (m_read_in_progress)
		CancelRead();
	if (m_fd != -1)
		close(m_fd);

	m_fd = -1;
}

uint FlatFileReader::GetBlockCount(void) const
{
	return (int)(Path::GetFileSize(m_filename) / m_blocksize);
}
