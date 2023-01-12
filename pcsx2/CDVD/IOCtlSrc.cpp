/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "CDVDdiscReader.h"
#include "CDVD.h"

#if defined(__linux__)
#include <linux/cdrom.h>
#elif defined(_WIN32)
#include <winioctl.h>
#include <ntddcdvd.h>
#include <ntddcdrm.h>
// "typedef ignored" warning will disappear once we move to the Windows 10 SDK.
#pragma warning(push)
#pragma warning(disable : 4091)
#include <ntddscsi.h>
#pragma warning(pop)
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <cstddef>
#include <cstdlib>
#include <climits>
#include <cstring> /* memcpy */
#include <stdexcept>

IOCtlSrc::IOCtlSrc(decltype(m_filename) filename)
	: m_filename(filename)
{
	if (!Reopen())
		throw std::runtime_error(" * CDVD: Error opening source.\n");
}

IOCtlSrc::~IOCtlSrc()
{
#if defined(__unix__) || defined(__APPLE__)
	if (m_device == -1)
		return;
#elif defined(_WIN32)
	if (m_device == INVALID_HANDLE_VALUE)
		return;
#endif
	SetSpindleSpeed(true);
#if defined(_WIN32)
	CloseHandle(m_device);
#elif defined(__unix__) || defined(__APPLE__)
	close(m_device);
#endif
}

// If a new disc is inserted, ReadFile will fail unless the device is closed
// and reopened.
bool IOCtlSrc::Reopen()
{
#if defined(__unix__) || defined(__APPLE__)
	if (m_device != -1)
		close(m_device);

	// O_NONBLOCK allows a valid file descriptor to be returned even if the
	// drive is empty. Probably does other things too.
	m_device = open(m_filename.c_str(), O_RDONLY | O_NONBLOCK);
	if (m_device == -1)
		return false;
#elif defined(_WIN32)
	DWORD unused;
	if (m_device != INVALID_HANDLE_VALUE)
		CloseHandle(m_device);

	// SPTI only works if the device is opened with GENERIC_WRITE access.
	m_device = CreateFileW(m_filename.c_str(), GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (m_device == INVALID_HANDLE_VALUE)
		return false;

	// Required to read from layer 1 of Dual layer DVDs
	DeviceIoControl(m_device, FSCTL_ALLOW_EXTENDED_DASD_IO,
			nullptr, 0, nullptr,
			0, &unused, nullptr);
#endif

	// DVD detection MUST be first on Linux - The TOC ioctls work for both
	// CDs and DVDs.
	if (ReadDVDInfo() || ReadCDInfo())
		SetSpindleSpeed(false);

	return true;
}

void IOCtlSrc::SetSpindleSpeed(bool restore_defaults) const
{
#if defined(_WIN32)
	// IOCTL_CDROM_SET_SPEED issues a SET CD SPEED command. So 0xFFFF should be
	// equivalent to "optimal performance".
	// 1x DVD-ROM and CD-ROM speeds are respectively 1385 KB/s and 150KB/s.
	// The PS2 can do 4x DVD-ROM and 24x CD-ROM speeds (5540KB/s and 3600KB/s).
	// TODO: What speed? Performance seems smoother with a lower speed (less
	// time required to get up to speed).
	DWORD unused;
	const USHORT speed = restore_defaults ? 0xFFFF : GetMediaType() >= 0 ? 5540 : 3600;
	CDROM_SET_SPEED s{CdromSetSpeed, speed, speed, CdromDefaultRotation};

	DeviceIoControl(m_device, IOCTL_CDROM_SET_SPEED, &s, sizeof(s),
			nullptr, 0, &unused, nullptr);
#endif
}

u32 IOCtlSrc::GetSectorCount() const
{
	return m_sectors;
}

u32 IOCtlSrc::GetLayerBreakAddress() const
{
	return m_layer_break;
}

s32 IOCtlSrc::GetMediaType() const
{
	return m_media_type;
}

const std::vector<toc_entry>& IOCtlSrc::ReadTOC() const
{
	return m_toc;
}

bool IOCtlSrc::ReadSectors2048(u32 sector, u32 count, u8* buffer) const
{
#if defined(__unix__) || defined(__APPLE__)
	const ssize_t bytes_to_read = 2048 * count;
	ssize_t bytes_read = pread(m_device, buffer, bytes_to_read, sector * 2048ULL);
	if (bytes_read == bytes_to_read)
		return true;
#elif defined(_WIN32)
	std::lock_guard<std::mutex> guard(m_lock);
	LARGE_INTEGER offset;
	offset.QuadPart = sector * 2048ULL;

	if (!SetFilePointerEx(m_device, offset, nullptr, FILE_BEGIN))
		return false;

	const DWORD bytes_to_read = 2048 * count;
	DWORD bytes_read;
	if (ReadFile(m_device, buffer, bytes_to_read, &bytes_read, nullptr))
		if (bytes_read == bytes_to_read)
			return true;
#endif
	return false;
}

bool IOCtlSrc::ReadSectors2352(u32 sector, u32 count, u8* buffer) const
{
#if defined(_WIN32)
	struct sptdinfo
	{
		SCSI_PASS_THROUGH_DIRECT info;
		char sense_buffer[20];
	} sptd{};

	// READ CD command
	sptd.info.Cdb[0] = 0xBE;
	// Don't care about sector type.
	sptd.info.Cdb[1] = 0;
	// Number of sectors to read
	sptd.info.Cdb[6] = 0;
	sptd.info.Cdb[7] = 0;
	sptd.info.Cdb[8] = 1;
	// Sync + all headers + user data + EDC/ECC. Excludes C2 + subchannel
	sptd.info.Cdb[9] = 0xF8;
	sptd.info.Cdb[10] = 0;
	sptd.info.Cdb[11] = 0;

	sptd.info.CdbLength = 12;
	sptd.info.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
	sptd.info.DataIn = SCSI_IOCTL_DATA_IN;
	sptd.info.SenseInfoOffset = offsetof(sptdinfo, sense_buffer);
	sptd.info.TimeOutValue = 5;

	// Read sectors one by one to avoid reading data from 2 tracks of different
	// types in the same read (which will fail).
	for (u32 n = 0; n < count; ++n)
	{
		u32 current_sector           = sector + n;
		sptd.info.Cdb[2]             = (current_sector >> 24) & 0xFF;
		sptd.info.Cdb[3]             = (current_sector >> 16) & 0xFF;
		sptd.info.Cdb[4]             = (current_sector >> 8) & 0xFF;
		sptd.info.Cdb[5]             = current_sector & 0xFF;
		sptd.info.DataTransferLength = 2352;
		sptd.info.DataBuffer         = buffer + 2352 * n;
		sptd.info.SenseInfoLength    = sizeof(sptd.sense_buffer);

		DWORD unused;
		if (DeviceIoControl(m_device, IOCTL_SCSI_PASS_THROUGH_DIRECT, &sptd,
					sizeof(sptd), &sptd, sizeof(sptd), &unused, nullptr))
		{
			if (sptd.info.DataTransferLength == 2352)
				continue;
		}
		return false;
	}

	return true;
#elif defined(__linux__)
	union
	{
		cdrom_msf msf;
		char buffer[CD_FRAMESIZE_RAW];
	} data;

	for (u32 n = 0; n < count; ++n)
	{
		u32 lba = sector + n;
		lba_to_msf(lba, &data.msf.cdmsf_min0, &data.msf.cdmsf_sec0, &data.msf.cdmsf_frame0);
		if (ioctl(m_device, CDROMREADRAW, &data) == -1)
			return false;
		memcpy(buffer, data.buffer, CD_FRAMESIZE_RAW);
		buffer += CD_FRAMESIZE_RAW;
	}

	return true;
#else
	return false;
#endif
}

bool IOCtlSrc::ReadDVDInfo(void)
{
#if defined(_WIN32)
	DWORD unused;
	// 4 bytes header + 18 bytes layer descriptor - Technically you only need
	// to read 17 bytes of the layer descriptor since bytes 17-2047 is for
	// media specific information. However, Windows requires you to read at
	// least 18 bytes of the layer descriptor or else the ioctl will fail. The
	// media specific information seems to be empty, so there's no point reading
	// any more than that.
	std::array<u8, 22> buffer;
	DVD_READ_STRUCTURE dvdrs{{0}, DvdPhysicalDescriptor, 0, 0};

	if (!DeviceIoControl(m_device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs),
						 buffer.data(), buffer.size(), &unused, nullptr))
		return false;

	auto& layer = *reinterpret_cast<DVD_LAYER_DESCRIPTOR*>(
		reinterpret_cast<DVD_DESCRIPTOR_HEADER*>(buffer.data())->Data);

	u32 start_sector = _byteswap_ulong(layer.StartingDataSector);
	u32 end_sector = _byteswap_ulong(layer.EndDataSector);

	if (layer.NumberOfLayers == 0)
	{
		// Single layer
		m_media_type = 0;
		m_layer_break = 0;
		m_sectors = end_sector - start_sector + 1;
	}
	else if (layer.TrackPath == 0)
	{
		// Dual layer, Parallel Track Path
		dvdrs.LayerNumber = 1;
		if (!DeviceIoControl(m_device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs),
							 buffer.data(), buffer.size(), &unused, nullptr))
			return false;
		u32 layer1_start_sector = _byteswap_ulong(layer.StartingDataSector);
		u32 layer1_end_sector = _byteswap_ulong(layer.EndDataSector);

		m_media_type = 1;
		m_layer_break = end_sector - start_sector;
		m_sectors = end_sector - start_sector + 1 + layer1_end_sector - layer1_start_sector + 1;
	}
	else
	{
		// Dual layer, Opposite Track Path
		u32 end_sector_layer0 = _byteswap_ulong(layer.EndLayerZeroSector);
		m_media_type = 2;
		m_layer_break = end_sector_layer0 - start_sector;
		m_sectors = end_sector_layer0 - start_sector + 1 + end_sector - (~end_sector_layer0 & 0xFFFFFFU) + 1;
	}

	return true;
#elif defined(__linux__)
	dvd_struct dvdrs;
	dvdrs.type = DVD_STRUCT_PHYSICAL;
	dvdrs.physical.layer_num = 0;

	int ret = ioctl(m_device, DVD_READ_STRUCT, &dvdrs);
	if (ret == -1)
		return false;

	u32 start_sector = dvdrs.physical.layer[0].start_sector;
	u32 end_sector = dvdrs.physical.layer[0].end_sector;

	if (dvdrs.physical.layer[0].nlayers == 0)
	{
		// Single layer
		m_media_type = 0;
		m_layer_break = 0;
		m_sectors = end_sector - start_sector + 1;
	}
	else if (dvdrs.physical.layer[0].track_path == 0)
	{
		// Dual layer, Parallel Track Path
		dvdrs.physical.layer_num = 1;
		ret = ioctl(m_device, DVD_READ_STRUCT, &dvdrs);
		if (ret == -1)
			return false;
		u32 layer1_start_sector = dvdrs.physical.layer[1].start_sector;
		u32 layer1_end_sector = dvdrs.physical.layer[1].end_sector;

		m_media_type = 1;
		m_layer_break = end_sector - start_sector;
		m_sectors = end_sector - start_sector + 1 + layer1_end_sector - layer1_start_sector + 1;
	}
	else
	{
		// Dual layer, Opposite Track Path
		u32 end_sector_layer0 = dvdrs.physical.layer[0].end_sector_l0;
		m_media_type = 2;
		m_layer_break = end_sector_layer0 - start_sector;
		m_sectors = end_sector_layer0 - start_sector + 1 + end_sector - (~end_sector_layer0 & 0xFFFFFFU) + 1;
	}

	return true;
#else
	return false;
#endif
}

bool IOCtlSrc::ReadCDInfo(void)
{
#if defined(_WIN32)
	DWORD unused;
	CDROM_READ_TOC_EX toc_ex{};
	toc_ex.Format = CDROM_READ_TOC_EX_FORMAT_TOC;
	toc_ex.Msf = 0;
	toc_ex.SessionTrack = 1;

	CDROM_TOC toc;
	if (!DeviceIoControl(m_device, IOCTL_CDROM_READ_TOC_EX, &toc_ex,
						 sizeof(toc_ex), &toc, sizeof(toc), &unused, nullptr))
		return false;

	m_toc.clear();
	size_t track_count = ((toc.Length[0] << 8) + toc.Length[1] - 2) / sizeof(TRACK_DATA);
	for (size_t n = 0; n < track_count; ++n)
	{
		TRACK_DATA& track = toc.TrackData[n];
		// Exclude the lead-out track descriptor.
		if (track.TrackNumber == 0xAA)
			continue;
		u32 lba = (track.Address[1] << 16) + (track.Address[2] << 8) + track.Address[3];
		m_toc.push_back({lba, track.TrackNumber, track.Adr, track.Control});
	}

	GET_LENGTH_INFORMATION info;
	if (!DeviceIoControl(m_device, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &info,
						 sizeof(info), &unused, nullptr))
		return false;

	m_sectors = static_cast<u32>(info.Length.QuadPart / 2048);
	m_media_type = -1;

	return true;
#elif defined(__linux__)
	cdrom_tochdr header;

	if (ioctl(m_device, CDROMREADTOCHDR, &header) == -1)
		return false;

	cdrom_tocentry entry{};
	entry.cdte_format = CDROM_LBA;

	m_toc.clear();
	for (u8 n = header.cdth_trk0; n <= header.cdth_trk1; ++n)
	{
		entry.cdte_track = n;
		if (ioctl(m_device, CDROMREADTOCENTRY, &entry) != -1)
			m_toc.push_back({static_cast<u32>(entry.cdte_addr.lba), entry.cdte_track,
							 entry.cdte_adr, entry.cdte_ctrl});
	}

	// TODO: Do I need a fallback if this doesn't work?
	entry.cdte_track = 0xAA;
	if (ioctl(m_device, CDROMREADTOCENTRY, &entry) == -1)
		return false;

	m_sectors = entry.cdte_addr.lba;
	m_media_type = -1;

	return true;
#else
	return false;
#endif
}

bool IOCtlSrc::DiscReady()
{
#if defined(_WIN32)
	DWORD unused;
	if (m_device == INVALID_HANDLE_VALUE)
		return false;
	if (DeviceIoControl(m_device, IOCTL_STORAGE_CHECK_VERIFY, nullptr, 0,
				nullptr, 0, &unused, nullptr))
	{
		if (!m_sectors)
			Reopen();
	}
	else
	{
		m_sectors     = 0;
		m_layer_break = 0;
		m_media_type  = 0;
	}

	return !!m_sectors;
#elif defined(__linux__)
	if (m_device == -1)
		return false;

	// CDSL_CURRENT must be used - 0 will cause the drive tray to close.
	if (ioctl(m_device, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK)
	{
		if (!m_sectors)
			Reopen();
	}
	else
	{
		m_sectors = 0;
		m_layer_break = 0;
		m_media_type = 0;
	}

	return !!m_sectors;
#else
	return false;
#endif
}
