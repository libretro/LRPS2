#include <wx/ffile.h>
#include "retro_messager.h"
#include <sys/types.h>
#include <stdio.h>
#include <string.h>


static const int MC2_MBSIZE = 1024 * 528 * 2;		// Size of a single megabyte of card data



namespace MemCardRetro
{

	// given a file path, return the raw filename without extension 
	wxString GetContentFileRawName(std::string& content_path)
	{
		size_t last_slash_index = content_path.find_last_of("\\/");
		std::string game_name = content_path.substr(last_slash_index + 1, content_path.length());
		size_t lastindex = game_name.find_last_of(".");
		std::string rawname = game_name.substr(0, lastindex);

		return wxString(rawname);
	}

	/*
	* This is a copy  of the function in gui/MemoryCardFile.cpp
	* I made a copy because I tried to include MemoryCardFile.h but it
	* gave me a mess of dependency errors which I wasn't able 
	* to resolve. Because only this function is needed, the best thing 
	* to do was simply doing a copy of the function - SeventySixx
	*/

	// returns FALSE if an error occurred (either permission denied or disk full)
	bool Create(const wxString& mcdFile, uint sizeInMB)
	{

		//uint sizeInMB = 8;
		u8 m_effeffs[528 * 16];
		memset8<0xff>(m_effeffs);

		Console.WriteLn(L"(FileMcd) Creating new %uMB memory card: " + mcdFile, sizeInMB);

		wxFFile fp(mcdFile, L"wb");
		if (!fp.IsOpened()) return false;

		for (uint i = 0; i < (MC2_MBSIZE * sizeInMB) / sizeof(m_effeffs); i++)
		{
			if (fp.Write(m_effeffs, sizeof(m_effeffs)) == 0)
				return false;
		}
		return true;
	}

	bool CreateSharedMemCardIfNotExisting(const wxFileName& memcard_file, uint sizeInMB)
	{
		if (!memcard_file.FileExists())
		{
			MemCardRetro::Create(memcard_file.GetFullPath(), sizeInMB);
			std::string msg = "Memory card created: ";
			std::string name = memcard_file.GetName().ToStdString();
			msg.append(name);
			RetroMessager::Notification(msg.c_str());
			return true;
		}
		return false;
	}


}
