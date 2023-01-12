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

#define _PC_ // disables MIPS opcode macros.

#include "Common.h"
#include "Patch.h"
#include "IopMem.h"
#include "GameDatabase.h"
#include "MemoryPatchDatabase.h"

#include <memory>
#include <vector>
#include <wx/textfile.h>
#include <wx/dir.h>

#include "cheats_ws.h"
#include "cheats_60fps.h"
#include "cheats_nointerlacing.h"

#include "retro_messager.h"

// This is a declaration for PatchMemory.cpp::_ApplyPatch where we're (patch.cpp)
// the only consumer, so it's not made public via Patch.h
// Applies a single patch line to emulation memory regardless of its "place" value.
extern void _ApplyPatch(IniPatch* p);

static std::vector<IniPatch> Patch;

struct PatchTextTable
{
	int				code;
	const char*	text;
	PATCHTABLEFUNC*	func;
};

static const PatchTextTable commands_patch[] =
{
	{ 1, "author",		PatchFunc::author},
	{ 2, "comment",	PatchFunc::comment },
	{ 3, "patch",		PatchFunc::patch },
	{ 0, "", NULL } // Array Terminator
};

static const PatchTextTable dataType[] =
{
	{ 1, "byte", NULL },
	{ 2, "short", NULL },
	{ 3, "word", NULL },
	{ 4, "double", NULL },
	{ 5, "extended", NULL },
	{ 0, "", NULL }
};

static const PatchTextTable cpuCore[] =
{
	{ 1, "EE", NULL },
	{ 2, "IOP", NULL },
	{ 0, "",  NULL }
};

// IniFile Functions.

static void inifile_trim(wxString& buffer)
{
	buffer.Trim(false); // trims left side.

	if (buffer.Length() <= 1) // this I'm not sure about... - air
	{
		buffer.Empty();
		return;
	}

	if (buffer.Left(2) == L"//")
	{
		buffer.Empty();
		return;
	}

	buffer.Trim(true); // trims right side.
}

static int PatchTableExecute(const ParsedAssignmentString& set, const PatchTextTable* Table)
{
	int i = 0;

	while (Table[i].text[0])
	{
		if (!set.lvalue.Cmp(Table[i].text))
		{
			if (Table[i].func)
				Table[i].func(set.lvalue, set.rvalue);
			break;
		}
		i++;
	}

	return Table[i].code;
}

// This routine is for executing the commands of the ini file.
static void inifile_command(const wxString& cmd)
{
	ParsedAssignmentString set(cmd);

	// Is this really what we want to be doing here? Seems like just leaving it empty/blank
	// would make more sense... --air
	if (set.rvalue.IsEmpty())
		set.rvalue = set.lvalue;

	/*int code = */ PatchTableExecute(set, commands_patch);
}

// This routine loads patches from the game database (but not the config/game fixes/hacks)
// Returns number of patches loaded
int LoadPatchesFromGamesDB(const wxString& crc, const GameDatabaseSchema::GameEntry& game)
{
	if (game.isValid)
	{
		GameDatabaseSchema::Patch patch;
		bool patchFound = game.findPatch(std::string(crc), patch);
		if (patchFound && patch.patchLines.size() > 0)
		{
			for (auto line : patch.patchLines)
			{
				inifile_command(line);
			}
		}
	}

	return Patch.size();
}

static void inifile_processString(const wxString& inStr)
{
	wxString str(inStr);
	inifile_trim(str);
	if (!str.IsEmpty())
		inifile_command(str);
}

// This routine receives a file from inifile_read, trims it,
// Then sends the command to be parsed.
static void inifile_process(wxTextFile& f1)
{
	for (uint i = 0; i < f1.GetLineCount(); i++)
		inifile_processString(f1[i]);
}

void ForgetLoadedPatches()
{
	Patch.clear();
}

static int _LoadPatchFiles(const wxDirName& folderName, wxString& fileSpec, const wxString& friendlyName, int& numberFoundPatchFiles)
{
	numberFoundPatchFiles = 0;

	if (!folderName.Exists())
		return 0;
	wxDir dir(folderName.ToString());

	int before = Patch.size();
	wxString buffer;
	wxTextFile f;
	bool found = dir.GetFirst(&buffer, L"*", wxDIR_FILES);
	while (found)
	{
		if (buffer.Upper().Matches(fileSpec.Upper()))
		{
			log_cb(RETRO_LOG_INFO, "Found %s file: '%s'\n", WX_STR(friendlyName), WX_STR(buffer));
			int before = Patch.size();
			f.Open(Path::Combine(dir.GetName(), buffer));
			inifile_process(f);
			f.Close();
			int loaded = Patch.size() - before;
			log_cb(RETRO_LOG_INFO, "Loaded %d %s from '%s' at '%s'\n",
				loaded, WX_STR(friendlyName), WX_STR(buffer), WX_STR(folderName.ToString()));
			numberFoundPatchFiles++;
		}
		found = dir.GetNext(&buffer);
	}

	return Patch.size() - before;
}

int Load60fpsPatchesFromDatabase(std::string gameCRC)
{
	static MemoryPatchDatabase *sixtyfps_database;
	std::transform(gameCRC.begin(), gameCRC.end(), gameCRC.begin(), ::toupper);

	int before = Patch.size();

	if (!sixtyfps_database)
	{
		sixtyfps_database = new MemoryPatchDatabase(cheats_60fps_zip, cheats_60fps_zip_len);
		sixtyfps_database->InitEntries();
	}
	std::vector<std::string> patch_lines = sixtyfps_database->GetPatchLines(gameCRC);

	for (std::string line : patch_lines)
		inifile_processString(line);

	return Patch.size() - before;
}

int LoadWidescreenPatchesFromDatabase(std::string gameCRC)
{
	static MemoryPatchDatabase *widescreen_database;
	std::transform(gameCRC.begin(), gameCRC.end(), gameCRC.begin(), ::toupper);

	int before = Patch.size();

	if (!widescreen_database)
	{
		widescreen_database = new MemoryPatchDatabase(cheats_ws_zip, cheats_ws_zip_len);
		widescreen_database->InitEntries();
	}
	std::vector<std::string> patch_lines = widescreen_database->GetPatchLines(gameCRC);

	for (std::string line : patch_lines)
		inifile_processString(line);

	return Patch.size() - before;
}

int LoadNointerlacingPatchesFromDatabase(std::string gameCRC)
{
	static MemoryPatchDatabase* nointerlacing_database;
	std::transform(gameCRC.begin(), gameCRC.end(), gameCRC.begin(), ::toupper);

	int before = Patch.size();

	if (!nointerlacing_database)
	{
		nointerlacing_database = new MemoryPatchDatabase(cheats_nointerlacing_zip, cheats_nointerlacing_zip_len);
		nointerlacing_database->InitEntries();
	}
	std::vector<std::string> patch_lines = nointerlacing_database->GetPatchLines(gameCRC);

	for (std::string line : patch_lines)
		inifile_processString(line);

	return Patch.size() - before;
}


// This routine loads patches from *.pnach files
// Returns number of patches loaded
// Note: does not reset previously loaded patches (use ForgetLoadedPatches() for that)
int LoadPatchesFromDir(wxString name, const wxDirName& folderName, const wxString& friendlyName)
{
	int numberFoundPatchFiles;
	wxString filespec = name + L"*.pnach";
	return _LoadPatchFiles(folderName, filespec, friendlyName, numberFoundPatchFiles);
}

static u32 StrToU32(const wxString& str, int base = 10)
{
	unsigned long l;
	str.ToULong(&l, base);
	return l;
}

static u64 StrToU64(const wxString& str, int base = 10)
{
	wxULongLong_t l;
	str.ToULongLong(&l, base);
	return l;
}

// PatchFunc Functions.
namespace PatchFunc
{
	void comment(const char *text1, const char *text2)
	{
		log_cb(RETRO_LOG_INFO, "comment: %s\n", text2);
	}

	void author(const char *text1, const char *text2)
	{
		log_cb(RETRO_LOG_INFO, "Author: %s\n", text2);
	}

	struct PatchPieces
	{
		wxArrayString m_pieces;

		PatchPieces(const wxString& param)
		{
			// Splits a string into parts and adds the parts into the given SafeList.
			// This list is not cleared, so concatenating many splits into a single large list is
			// the 'default' behavior, unless you manually clear the SafeList prior to subsequent calls.
			wxStringTokenizer parts(param, L",", wxTOKEN_RET_EMPTY_ALL);
			while (parts.HasMoreTokens())
				m_pieces.Add(parts.GetNextToken());
		}

		const wxString& PlaceToPatch() const { return m_pieces[0]; }
		const wxString& CpuType() const { return m_pieces[1]; }
		const wxString& MemAddr() const { return m_pieces[2]; }
		const wxString& OperandSize() const { return m_pieces[3]; }
		const wxString& WriteValue() const { return m_pieces[4]; }
	};

	void patchHelper(const char *cmd, const char *param)
	{
		// Error Handling Note:  I just throw simple wxStrings here, and then catch them below and
		// format them into more detailed cmd+data+error printouts.  If we want to add user-friendly
		// (translated) messages for display in a popup window then we'll have to upgrade the
		// exception a little bit.
		{
			PatchPieces pieces(param);

			IniPatch iPatch     = {0};
			iPatch.enabled      = 0;
			iPatch.placetopatch = StrToU32(pieces.PlaceToPatch(), 10);

			if (iPatch.placetopatch >= _PPT_END_MARKER)
				goto error;

			iPatch.cpu  = (patch_cpu_type)PatchTableExecute(pieces.CpuType(), cpuCore);
			iPatch.addr = StrToU32(pieces.MemAddr(), 16);
			iPatch.type = (patch_data_type)PatchTableExecute(pieces.OperandSize(), dataType);
			iPatch.data = StrToU64(pieces.WriteValue(), 16);

			if (iPatch.cpu == 0)
			{
				log_cb(RETRO_LOG_ERROR, "Unrecognized CPU Target: '%s'\n", WX_STR(pieces.CpuType()));
				goto error;
			}

			if (iPatch.type == 0)
			{
				log_cb(RETRO_LOG_ERROR, "Unrecognized Operand Size: '%s'\n", WX_STR(pieces.OperandSize()));
				goto error;
			}

			iPatch.enabled = 1; // omg success!!
			Patch.push_back(iPatch);
		}

		return;
error:
		log_cb(RETRO_LOG_ERROR, "(Patch) Error Parsing: %s=%s\n", cmd, param);
	}
	void patch(const char *cmd, const char *param) { patchHelper(cmd, param); }
} // namespace PatchFunc

// This is for applying patches directly to memory
void ApplyLoadedPatches(patch_place_type place)
{
	for (auto& i : Patch)
	{
		if (i.placetopatch == place)
			_ApplyPatch(&i);
	}
}
