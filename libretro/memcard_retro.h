#include <wx/ffile.h>
#include "retro_messager.h"
#include <sys/types.h>
#include <stdio.h>
#include <string.h>


static const int MC2_MBSIZE = 1024 * 528 * 2;		// Size of a single megabyte of card data



namespace MemCardRetro
{
	bool isMemCardExisting(const char* mcdFile) {
		struct stat buffer;
		//std::string path = (std::string)mcdFile;
		return (stat(mcdFile, &buffer) == 0);
	}

	bool IsAMemoryCard(wxString mcdFile) {

		std::string file_path = (std::string)mcdFile;
		int position = file_path.find_last_of(".");
		std::string ext = file_path.substr(position + 1);

		return ext.compare("ps2")==0;
	}


	// given a file path, return the raw filename without extension 
	std::string getMemCardFileRawName(const char* game)
	{
		std::string game_path = game;
		size_t last_slash_index = game_path.find_last_of("\\/");
		std::string game_name = game_path.substr(last_slash_index + 1, game_path.length());
		//log_cb(RETRO_LOG_DEBUG, "file processed: %s\n", game_name.c_str());

		size_t lastindex = game_name.find_last_of(".");
		std::string rawname = game_name.substr(0, lastindex);
		//log_cb(RETRO_LOG_DEBUG, "Isolated savegame name: %s\n", rawname.c_str());

		return rawname;
	}



	std::string checkSaveDir(const std::string &path_saves, const std::string &subfolder_to_check)
	{
		//Judge whether the folder exists, create it if it does not exist
		std::string  path_saves_pcsx2 = path_saves;
		path_saves_pcsx2.append(subfolder_to_check);

		/*
		static char path_saves_pcsx2[300];
		strcpy(path_saves_pcsx2, path_saves);
		strcat(path_saves_pcsx2, subfolder_to_check);
		puts(path_saves_pcsx2);
		*/


		if (_access(path_saves_pcsx2.c_str(), 0) == -1)
		{
			_mkdir(path_saves_pcsx2.c_str());
			
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
		//log_cb(RETRO_LOG_DEBUG, "file processed: %s\n", game_name.c_str());

		size_t lastindex = game_name.find_last_of(".");
		std::string rawname = game_name.substr(0, lastindex);
		rawname.insert(0, "//");
		rawname.append(".ps2");
		//log_cb(RETRO_LOG_DEBUG, "Isolated savegame name: %s\n", rawname.c_str());

		return rawname.c_str();
	}


	const char* GetSharedMemCardFileName(const char* file_name, int number)
	{
		std::string rawname = file_name;
		rawname.insert(0, "//");
		rawname.append(" " + std::to_string(number) + ".ps2");
		//log_cb(RETRO_LOG_DEBUG, "Isolated savegame name: %s\n", rawname.c_str());

		return rawname.c_str();
	}


	const char* GetDefaultMemoryCardPath1(const char* system_path)
	{
		std::string sys_path(system_path);
		sys_path.append("//pcsx2/memcards//Mcd001.ps2");
	
		return (const char*)sys_path.c_str();
	}

	const char* GetDefaultMemoryCardPath2(const char* system_path)
	{
		std::string sys_path(system_path);
		sys_path.append("//pcsx2/memcards//Mcd002.ps2");

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

	bool CreateSharedMemCardIfNotExisting(const char* slot_dir, const char* memcard_name, uint sizeInMB)
	{
		const char* memcard_path = MemCardRetro::CreateMemCardFilePath(slot_dir, memcard_name);
		if (!MemCardRetro::isMemCardExisting(memcard_path))
		{
			MemCardRetro::Create(memcard_path, sizeInMB);
			std::string msg = "Memory card created: ";
			std::string name = memcard_name;
			msg.append(name);
			RetroMessager::Notification(msg.c_str());
			return true;
		}
		return false;
	}


}