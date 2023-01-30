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

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
static std::vector<std::wstring> GetOpticalDriveList(void)
{
	DWORD size = GetLogicalDriveStrings(0, nullptr);
	std::vector<wchar_t> drive_strings(size);
	if (GetLogicalDriveStringsW(size, drive_strings.data()) != size - 1)
		return {};

	std::vector<std::wstring> drives;
	for (auto p = drive_strings.data(); *p; ++p)
	{
		if (GetDriveTypeW(p) == DRIVE_CDROM)
			drives.push_back(p);
		while (*p)
			++p;
	}
	return drives;
}

void GetValidDrive(std::wstring& drive)
{
	if (drive.empty() || GetDriveTypeW(drive.c_str()) != DRIVE_CDROM)
	{
		auto drives = GetOpticalDriveList();
		if (drives.empty())
		{
			drive.clear();
			return;
		}
		drive = drives.front();
	}

	int size = WideCharToMultiByte(CP_UTF8, 0, drive.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::vector<char> converted_string(size);
	WideCharToMultiByte(CP_UTF8, 0, drive.c_str(), -1, converted_string.data(), converted_string.size(), nullptr, nullptr);

	// The drive string has the form "X:\", but to open the drive, the string
	// has to be in the form "\\.\X:"
	drive.pop_back();
	drive.insert(0, L"\\\\.\\");
}
#elif defined(__unix__) || defined(__APPLE__)
static std::vector<std::string> GetOpticalDriveList(void)
{
	return {};
}

void GetValidDrive(std::string& drive)
{
	if (!drive.empty())
		drive.clear();
	if (drive.empty())
	{
		auto drives = GetOpticalDriveList();
		if (!drives.empty())
			drive = drives.front();
	}
}
#endif
