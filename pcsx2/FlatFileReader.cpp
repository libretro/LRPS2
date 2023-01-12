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

#include "Utilities/Path.h"
#include "AsyncFileReader.h"

// The AIO module has been reported to cause issues with FreeBSD 10.3, so let's
// disable it for 10.3 and earlier and hope FreeBSD 11 and onwards is fine.
// Note: It may be worth checking whether aio provides any performance benefit.
#if defined(__FreeBSD__) && __FreeBSD__ < 11
#define DISABLE_AIO
#warning AIO has been disabled.
#endif

FlatFileReader::FlatFileReader(bool shareWrite) : shareWrite(shareWrite)
{
	m_blocksize        = 2048;
#ifdef _WIN32
	hOverlappedFile    = INVALID_HANDLE_VALUE;
	hEvent             = INVALID_HANDLE_VALUE;
#elif defined(__APPLE__) || defined(__unix__)
	m_fd               = -1;
#ifndef __APPLE__
	m_aio_context      = 0;
#endif
#endif
	m_read_in_progress = false;
}

FlatFileReader::~FlatFileReader(void)
{
	Close();
}

bool FlatFileReader::Open(const wxString& fileName)
{
	m_filename      = fileName;
#ifdef _WIN32
	hEvent          = CreateEvent(NULL, TRUE, FALSE, NULL);
	DWORD shareMode = FILE_SHARE_READ;
	if (shareWrite)
		shareMode |= FILE_SHARE_WRITE;

	hOverlappedFile = CreateFile(
		fileName,
		GENERIC_READ,
		shareMode,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED,
		NULL);

	return hOverlappedFile != INVALID_HANDLE_VALUE;
#elif defined(__APPLE__) || defined(__unix__)
#if !defined(__APPLE__)
	int err = io_setup(64, &m_aio_context);
	if (err)
		return false;
#endif
	m_fd = wxOpen(fileName, O_RDONLY, 0);
	return (m_fd != -1);
#endif
}

void FlatFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
#ifdef _WIN32
	LARGE_INTEGER offset;
	offset.QuadPart                  = sector * (s64)m_blocksize + m_dataoffset;
	
	DWORD bytesToRead                = count * m_blocksize;

	ZeroMemory(&asyncOperationContext, sizeof(asyncOperationContext));
	asyncOperationContext.hEvent     = hEvent;
	asyncOperationContext.Offset     = offset.LowPart;
	asyncOperationContext.OffsetHigh = offset.HighPart;

	ReadFile(hOverlappedFile, pBuffer, bytesToRead, NULL, &asyncOperationContext);
#elif defined(__APPLE__)
	u64 offset         = sector * (u64)m_blocksize + m_dataoffset;
	u32 bytesToRead    = count * m_blocksize;
#if defined(DISABLE_AIO)
	m_aiocb.aio_nbytes = pread(m_fd, pBuffer, bytesToRead, offset);
	if (m_aiocb.aio_nbytes != bytesToRead)
		m_aiocb.aio_nbytes = -1;
#else
	m_aiocb            = {0};
	m_aiocb.aio_fildes = m_fd;
	m_aiocb.aio_offset = offset;
	m_aiocb.aio_nbytes = bytesToRead;
	m_aiocb.aio_buf    = pBuffer;

	if (aio_read(&m_aiocb) != 0)
		return;
#endif
#elif defined(__unix__)
	struct iocb iocb;
	u64 offset      = sector * (s64)m_blocksize + m_dataoffset;
	u32 bytesToRead = count * m_blocksize;
	struct iocb* iocbs = &iocb;
	io_prep_pread(&iocb, m_fd, pBuffer, bytesToRead, offset);
	io_submit(m_aio_context, 1, &iocbs);
#endif
	m_read_in_progress = true;
}

int FlatFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	BeginRead(pBuffer, sector, count);
	return FinishRead();
}


int FlatFileReader::FinishRead(void)
{
#ifdef _WIN32
	DWORD bytes;
	if(!GetOverlappedResult(hOverlappedFile, &asyncOperationContext, &bytes, TRUE))
	{
		m_read_in_progress = false;
		return -1;
	}

	m_read_in_progress = false;
	return bytes;
#elif defined(__APPLE__)
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
#elif defined(__unix__)
	int max_nr = 1;
	struct io_event events[max_nr];
	int min_nr = 1;
	int event  = io_getevents(m_aio_context, min_nr, max_nr, events, NULL);
	if (event < 1)
		return -1;
	return 1;
#endif
}

void FlatFileReader::CancelRead(void)
{
#ifdef _WIN32
	CancelIo(hOverlappedFile);
#elif defined(__APPLE__)
#if !defined(DISABLE_AIO)
	aio_cancel(m_fd, &m_aiocb);
#endif
	m_read_in_progress = false;
#endif
}

void FlatFileReader::Close(void)
{
#if defined(_WIN32) || defined(__APPLE__)
	if (m_read_in_progress)
		CancelRead();
#endif
#ifdef _WIN32
	if(hOverlappedFile != INVALID_HANDLE_VALUE)
		CloseHandle(hOverlappedFile);
	if(hEvent != INVALID_HANDLE_VALUE)
		CloseHandle(hEvent);
	hOverlappedFile = INVALID_HANDLE_VALUE;
	hEvent          = INVALID_HANDLE_VALUE;
#elif defined(__APPLE__) || defined(__unix__)
	if (m_fd != -1)
		close(m_fd);
	m_fd            = -1;
#if !defined(__APPLE__)
	io_destroy(m_aio_context);
	m_aio_context   = 0;
#endif
#endif
}

uint FlatFileReader::GetBlockCount(void) const
{
#ifdef _WIN32
	LARGE_INTEGER fileSize;
	fileSize.LowPart = GetFileSize(hOverlappedFile, (DWORD*)&(fileSize.HighPart));
	return (int)(fileSize.QuadPart / m_blocksize);
#else
	return (int)(Path::GetFileSize(m_filename) / m_blocksize);
#endif
}
