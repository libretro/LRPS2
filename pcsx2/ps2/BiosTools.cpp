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

#include "Common.h"
#include "BiosTools.h"

#include <wx/mstream.h>
#include <wx/wfstream.h>

#include "Utilities/pxStreams.h"
// FIXME: Temporary hack until we remove dependence on Pcsx2App.
#include "AppConfig.h"
#include "../../libretro/options_tools.h"

#define DIRENTRY_SIZE 16

// --------------------------------------------------------------------------------------
// romdir structure (packing required!)
// --------------------------------------------------------------------------------------
//
#if defined(_MSC_VER)
#	pragma pack(1)
#endif

struct romdir
{
	char fileName[10];
	u16 extInfoSize;
	u32 fileSize;
} __packed;			// +16

#ifdef _MSC_VER
#	pragma pack()
#endif

struct BiosDebugInformation
{
	u32 biosVersion;
	u32 biosChecksum;
	u32 threadListAddr;
};

u32 BiosVersion;
u32 BiosChecksum;
wxString BiosDescription;

static const BiosDebugInformation* CurrentBiosInformation;

static const BiosDebugInformation biosVersions[] = {
	// USA     v02.00(14/06/2004)  Console
	{ 0x00000200, 0xD778DB8D, 0x8001a640 },
	// Europe  v02.00(14/06/2004)
	{ 0x00000200, 0X9C7B59D3, 0x8001a640 },
};

// This method throws a BadStream exception if the bios information chould not be obtained.
//  (indicating that the file is invalid, incomplete, corrupted, or plain naughty).
static void LoadBiosVersion( pxInputStream& fp, u32& version, wxString& description, wxString& zoneStr )
{
	uint i;
	romdir rd;

	for (i=0; i<512*1024; i++)
	{
		fp.Read( rd );
		if (strncmp( rd.fileName, "RESET", 5 ) == 0)
			break; /* found romdir */
	}

	if (i == 512*1024)
	{
		throw Exception::BadStream( fp.GetStreamName() )
			.SetDiagMsg(L"BIOS version check failed: 'RESET' tag could not be found.")
			.SetUserMsg(L"The selected BIOS file is not a valid PS2 BIOS.  Please re-configure.");
	}

	uint fileOffset = 0;

	while(strlen(rd.fileName) > 0)
	{
		if (strcmp(rd.fileName, "ROMVER") == 0)
		{
			char romver[14+1];		// ascii version loaded from disk.

			wxFileOffset filetablepos = fp.Tell();
			fp.Seek( fileOffset );
			fp.Read( &romver, 14 );
			fp.Seek( filetablepos );	//go back

			romver[14] = 0;

			const char zonefail[2] = { romver[4], '\0' };	// the default "zone" (unknown code)
			const char* zone = zonefail;

			switch(romver[4])
			{
				case 'T': zone = "T10K";	break;
				case 'X': zone = "Test";	break;
				case 'J': zone = "Japan";	break;
				case 'A': zone = "USA";		break;
				case 'E': zone = "Europe";	break;
				case 'H': zone = "HK";		break;
				case 'P': zone = "Free";	break;
				case 'C': zone = "China";	break;
			}

			char vermaj[3] = { romver[0], romver[1], 0 };
			char vermin[3] = { romver[2], romver[3], 0 };

			FastFormatUnicode result;
			result.Write( "%-7s v%s.%s(%c%c/%c%c/%c%c%c%c)  %s",
				zone,
				vermaj, vermin,
				romver[12], romver[13],	// day
				romver[10], romver[11],	// month
				romver[6], romver[7], romver[8], romver[9],	// year!
				(romver[5]=='C') ? "Console" : (romver[5]=='D') ? "Devel" : ""
			);

			version = strtol(vermaj, (char**)NULL, 0) << 8;
			version|= strtol(vermin, (char**)NULL, 0);

			log_cb(RETRO_LOG_INFO, "Bios Found: %ls\n", result.c_str());

			description = result.c_str();
			zoneStr = fromUTF8(zone);
		}

		if ((rd.fileSize % 0x10) == 0)
			fileOffset += rd.fileSize;
		else
			fileOffset += (rd.fileSize + 0x10) & 0xfffffff0;

		fp.Read( rd );
	}

	fileOffset -= ((rd.fileSize + 0x10) & 0xfffffff0) - rd.fileSize;

	if (description.IsEmpty())
		throw Exception::BadStream( fp.GetStreamName() )
			.SetDiagMsg(L"BIOS version check failed: 'ROMDIR' tag could not be found.")
			.SetUserMsg(L"The selected BIOS file is not a valid PS2 BIOS.  Please re-configure.");

	wxFileOffset fileSize = fp.Length();
	if (fileSize < (int)fileOffset)
	{
		description += pxsFmt( L" %d%%", ((fileSize*100) / (int)fileOffset) );
		// we force users to have correct bioses,
		// not that lame scph10000 of 513KB ;-)
	}
}

template< size_t _size >
static void ChecksumIt( u32& result, const u8 (&srcdata)[_size] )
{
	for( size_t i=0; i<_size/4; ++i )
		result ^= ((u32*)srcdata)[i];
}

// Attempts to load a BIOS rom sub-component, by trying multiple combinations of base
// filename and extension.  The bios specified in the user's configuration is used as
// the base.
//
// Parameters:
//   ext - extension of the sub-component to load.  Valid options are rom1 and rom2.
//
template< size_t _size >
static void LoadExtraRom( const char *ext, u8 (&dest)[_size] )
{
	wxString Bios1;
	s64 filesize = 0;

	// Try first a basic extension concatenation (normally results in something like name.bin.rom1)
	const wxString Bios( g_Conf->FullpathToBios() );
	Bios1.Printf( L"%s.%s", WX_STR(Bios), ext);

	try
	{
		if( (filesize=Path::GetFileSize( Bios1 ) ) <= 0 )
		{
			// Try the name properly extensioned next (name.rom1)
			wxFileName jojo(Bios);
			jojo.SetExt(ext);
			Bios1 = jojo.GetFullPath();
			if( (filesize=Path::GetFileSize( Bios1 ) ) <= 0 )
			{
				log_cb(RETRO_LOG_INFO, "BIOS %s module not found, skipping...\n", ext );
				return;
			}
		}

		wxFile fp( Bios1 );
		fp.Read( dest, std::min<s64>( _size, filesize ) );
	}
	catch (Exception::BadStream& ex)
	{
		// If any of the secondary roms fail,its a non-critical error.
		// Log it, but don't make a big stink.  99% of games and stuff will
		// still work fine.

		log_cb(RETRO_LOG_WARN, "BIOS Warning: %s could not be read (permission denied?)\n", ext);
		log_cb(RETRO_LOG_INFO, "File size: %llu\n", filesize);
	}
}

static void LoadIrx( const wxString& filename, u8* dest )
{
	s64 filesize = 0;
	try
	{
		wxFile irx(filename);
		if( (filesize=Path::GetFileSize( filename ) ) <= 0 ) {
			log_cb(RETRO_LOG_WARN, "IRX Warning: %s could not be read\n", WX_STR(filename));
			return;
		}

		irx.Read( dest, filesize );
	}
	catch (Exception::BadStream& ex)
	{
		log_cb(RETRO_LOG_WARN, "IRX Warning: %s could not be read\n", WX_STR(filename));
	}
}

// Loads the configured bios rom file into PS2 memory.  PS2 memory must be allocated prior to
// this method being called.
//
// Remarks:
//   This function does not fail if rom1 or rom2 files are missing, since none are
//   explicitly required for most emulation tasks.
//
// Exceptions:
//   BadStream - Thrown if the primary bios file (usually .bin) is not found, corrupted, etc.
//
void LoadBIOS(void)
{
	try
	{
		wxString Bios( g_Conf->FullpathToBios() );
		if( !g_Conf->BaseFilenames.Bios.IsOk() || g_Conf->BaseFilenames.Bios.IsDir() )
			throw Exception::FileNotFound( Bios )
				.SetDiagMsg(L"BIOS has not been configured, or the configuration has been corrupted.")
				.SetUserMsg(L"The PS2 BIOS could not be loaded.  The BIOS has not been configured, or the configuration has been corrupted.  Please re-configure.");

		s64 filesize = Path::GetFileSize( Bios );
		if( filesize <= 0 )
		{
			throw Exception::FileNotFound( Bios )
				.SetDiagMsg(L"Configured BIOS file does not exist, or has a file size of zero.")
				.SetUserMsg(L"The configured BIOS file does not exist.  Please re-configure.");
		}

		BiosChecksum = 0;

		wxString biosZone;
		wxFFile fp( Bios , "rb");
		fp.Read( eeMem->ROM, std::min<s64>( Ps2MemSize::Rom, filesize ) );

		ChecksumIt( BiosChecksum, eeMem->ROM );

		pxInputStream memfp( Bios, new wxMemoryInputStream( eeMem->ROM, sizeof(eeMem->ROM) ) );
		LoadBiosVersion( memfp, BiosVersion, BiosDescription, biosZone );

		LoadExtraRom( "rom1", eeMem->ROM1 );
		LoadExtraRom( "rom2", eeMem->ROM2 );

		if (g_Conf->CurrentIRX.Length() > 3)
			LoadIrx(g_Conf->CurrentIRX, &eeMem->ROM[0x3C0000]);

		CurrentBiosInformation = NULL;
		for (size_t i = 0; i < sizeof(biosVersions)/sizeof(biosVersions[0]); i++)
		{
			if (biosVersions[i].biosChecksum == BiosChecksum && biosVersions[i].biosVersion == BiosVersion)
			{
				CurrentBiosInformation = &biosVersions[i];
				break;
			}
		}
	}
	catch (Exception::BadStream& ex)
	{
		// BIOS Load failure
	}
}

bool IsBIOS(const wxString& filename, wxString& description)
{
	pxInputStream inway( filename, new wxFFileInputStream( filename ) );

	if (!inway.IsOk()) return false;
	// FPS2BIOS is smaller and of variable size
	//if (inway.Length() < 512*1024) return false;

	try {
		u32 version;
		wxString zoneStr;
		LoadBiosVersion( inway, version, description, zoneStr );
		return true;
	} catch( Exception::BadStream& ) { }

	return false;	// fail quietly
}
