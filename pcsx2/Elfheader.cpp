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

#include "Elfheader.h"
#include "Utilities/Dependencies.h"
#include "CDVD/IsoFS/IsoFile.h"
#include "DebugTools/SymbolMap.h"

#include <wx/longlong.h>

#define ELF32_ST_TYPE(i) ((i)&0xf)

u32 ElfCRC;
u32 ElfEntry;
std::pair<u32,u32> ElfTextRange;
wxString LastELF;

// All of ElfObjects functions.
ElfObject::ElfObject(const wxString& srcfile, IsoFile& isofile)
	: data( wxULongLong(isofile.getLength()).GetLo())
	, proghead( NULL )
	, secthead( NULL )
	, filename( srcfile )
	, header( *(ELF_HEADER*)data.GetPtr() )
{
	isCdvd = true;
	readIso(isofile);
	initElfHeaders();
}

ElfObject::ElfObject( const wxString& srcfile, uint hdrsize )
	: data( wxULongLong(hdrsize).GetLo() )
	, proghead( NULL )
	, secthead( NULL )
	, filename( srcfile )
	, header( *(ELF_HEADER*)data.GetPtr() )
{
	isCdvd = false;
	readFile();
	initElfHeaders();
}

void ElfObject::initElfHeaders()
{
	if ( header.e_phnum > 0 )
	{
		if ((header.e_phoff + sizeof(ELF_PHR)) <= data.GetSizeInBytes())
			proghead = (ELF_PHR*)&data[header.e_phoff];
	}

	if ( header.e_shnum > 0 )
	{
		if ((header.e_shoff + sizeof(ELF_SHR)) <= data.GetSizeInBytes())
			secthead = (ELF_SHR*)&data[header.e_shoff];
	}
}

std::pair<u32,u32> ElfObject::getTextRange()
{
	for (int i = 0; i < header.e_phnum; i++)
	{
		u32 start = proghead[i].p_vaddr;
		u32 size = proghead[i].p_memsz;

		if (start <= header.e_entry && (start+size) > header.e_entry)
			return std::make_pair(start,size);
	}
	
	return std::make_pair(0,0);
}

void ElfObject::readIso(IsoFile& isofile)
{
	int rsize = isofile.read(data.GetPtr(), data.GetSizeInBytes());
	if (rsize < data.GetSizeInBytes())
		throw Exception::EndOfStream(filename);
}

void ElfObject::readFile()
{
	int rsize = 0;
	FILE *f = fopen( filename, "rb" );
	if (!f)
		throw Exception::FileNotFound( filename );

	fseek(f, 0, SEEK_SET);
	rsize = fread(data.GetPtr(), 1, data.GetSizeInBytes(), f);
	fclose( f );

	if (rsize < data.GetSizeInBytes()) throw Exception::EndOfStream(filename);
}

u32 ElfObject::getCRC()
{
	u32 CRC = 0;

	const u32* srcdata = (u32*)data.GetPtr();
	for(u32 i=data.GetSizeInBytes()/4; i; --i, ++srcdata)
		CRC ^= *srcdata;

	return CRC;
}

void ElfObject::loadHeaders()
{
	int i_st = -1, i_dt = -1;
	if (secthead == NULL || header.e_shoff > (u32)data.GetLength()) return;

	data.GetPtr( secthead[ (header.e_shstrndx == 0xffff ? 0 : header.e_shstrndx) ].sh_offset );

	for( int i = 0 ; i < header.e_shnum ; i++ )
	{
		// dump symbol table
		if (secthead[ i ].sh_type == 0x02)
		{
			i_st = i;
			i_dt = secthead[i].sh_link;
		}
	}

	if ((i_st >= 0) && (i_dt >= 0))
	{
		const char *SymNames = (char*)data.GetPtr(secthead[i_dt].sh_offset);
		Elf32_Sym *eS = (Elf32_Sym*)data.GetPtr(secthead[i_st].sh_offset);
		for(uint i = 1; i < (secthead[i_st].sh_size / sizeof(Elf32_Sym)); i++) {
			if ((eS[i].st_value != 0) && (ELF32_ST_TYPE(eS[i].st_info) == 2))
				symbolMap.AddLabel(&SymNames[eS[i].st_name],eS[i].st_value);
		}
	}
}

// return value:
//   0 - Invalid or unknown disc.
//   1 - PS1 CD
//   2 - PS2 CD
int GetPS2ElfName( wxString& name )
{
	int retype = 0;

	try {
		IsoFSCDVD isofs;
		IsoFile file( isofs, L"SYSTEM.CNF;1");

		int size = file.getLength();
		if( size == 0 )
			return 0;

		while( !file.eof() )
		{
			const wxString original( fromUTF8(file.readLine().c_str()) );
			const ParsedAssignmentString parts( original );

			if( parts.lvalue.IsEmpty() &&
				parts.rvalue.IsEmpty() )
				continue;
			/* Some games have a character on the last line of the file, don't print the error in those cases. Unusual or malformed entry in SYSTEM.CNF ignored */
			if( parts.rvalue.IsEmpty() && 
				file.getLength() != file.getSeekPos() )
				continue;

			if( parts.lvalue == L"BOOT2" ) /* Detected PS2 disc */
			{
				name   = parts.rvalue;
				retype = 2;
			}
			else if( parts.lvalue == L"BOOT" ) /* Detected PSX/PSone Disc */
			{
				name   = parts.rvalue;
				retype = 1;
			}
		}

		if( retype == 0 ) /* Disc image is NOT a PS1 or PS2 game */
			return 0;
	}
	catch( Exception::FileNotFound& )
	{
		return 0;		// no SYSTEM.CNF, not a PS1/PS2 disc.
	}
	catch (Exception::BadStream& ex)
	{
		return 0;		// ISO error
	}

	return retype;
}
