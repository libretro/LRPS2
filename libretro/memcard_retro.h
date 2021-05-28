#include <wx/ffile.h>
#include "retro_messager.h"
#include <sys/types.h>
#include <stdio.h>
#include <string.h>


static const int MC2_MBSIZE = 1024 * 528 * 2;		// Size of a single megabyte of card data
u8 m_effeffs[528 * 16];

namespace MemCardRetro
{
	bool isMemCardExisting(const char* mcdFile) {
		struct stat buffer;
		//std::string path = (std::string)mcdFile;
		return (stat(mcdFile, &buffer) == 0);
	}



	const char* checkSaveDir(const char* path_saves)
	{
		//Judge whether the folder exists, create it if it does not exist

		//const char* dir = "D:\\ASDF\\123.txt"; //_access can also be used to determine whether the file exists


		static char path_saves_pcsx2[300];
		strcpy(path_saves_pcsx2, path_saves);
		strcat(path_saves_pcsx2, "//pcsx2");
		puts(path_saves_pcsx2);



		if (_access(path_saves_pcsx2, 0) == -1)
		{
			_mkdir(path_saves_pcsx2);
			
		}
		return path_saves_pcsx2;
	}


	const char* CreateMemCardFilePath(const char* path_saves, const char* fileName)
	{
		//Judge whether the folder exists, create it if it does not exist

		//const char* dir = "D:\\ASDF\\123.txt"; //_access can also be used to determine whether the file exists


		static char path_saves_pcsx2[300];
		strcpy(path_saves_pcsx2, path_saves);
		strcat(path_saves_pcsx2, fileName);
		puts(path_saves_pcsx2);



		return path_saves_pcsx2;
	}

	//game_paths[0]

	const char* getMemCardFileName(wxString game)
	{
		std::string game_path = (std::string)game;
		size_t last_slash_index = game_path.find_last_of("\\/");
		std::string game_name = game_path.substr(last_slash_index + 1, game_path.length());
		log_cb(RETRO_LOG_DEBUG, "file processed: %s\n", game_name.c_str());

		size_t lastindex = game_name.find_last_of(".");
		std::string rawname = game_name.substr(0, lastindex);
		rawname.insert(0, "//");
		rawname.append(".ps2");
		log_cb(RETRO_LOG_DEBUG, "Isolated savegame name: %s\n", rawname.c_str());
		return rawname.c_str();
	}


	const char* GetDefaultMemoryCardPath(const char* system_path)
	{
		std::string sys_path(system_path);
		sys_path.append("//pcsx2/memcards//Mcd001.ps2");
	
		return (const char*)sys_path.c_str();
	}



	/*
	* This is a copy  of the fucntion in gui-libretro/MemoryCardFile.cpp
	* I made a copy because I tried to include MemoryCardFile.h but it
	* gave me a mess of dependency errors which I wasn't able 
	* to resolve. Because only this function is needed, the best thing 
	* to do was simply doing a copy of the function - SeventySixx
	*/

	// returns FALSE if an error occurred (either permission denied or disk full)
	bool Create(const wxString& mcdFile)
	{

		uint sizeInMB = 8;

		Console.WriteLn(L"(FileMcd) Creating new %uMB memory card: " + mcdFile, sizeInMB);

		wxFFile fp(mcdFile, L"wb");
		if (!fp.IsOpened()) return false;

		for (uint i = 0; i < (MC2_MBSIZE * sizeInMB) / sizeof(m_effeffs); i++)
		{
			if (fp.Write(m_effeffs, sizeof(m_effeffs)) == 0)
				return false;
		}
		RetroMessager::Notification("Memory Card created for the content", true);
		return true;
	}


}