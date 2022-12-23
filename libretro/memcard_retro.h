#include <sys/types.h>
#include <string.h>

#include <wx/ffile.h>

#include "Utilities/MemcpyFast.h"

#include "retro_messager.h"

#define MC2_MBSIZE 1081344 // Size of a single megabyte of card data

namespace MemCardRetro
{
	/*
	* This is a copy  of the function in gui-libretro/MemoryCardFile.cpp
	* I made a copy because I tried to include MemoryCardFile.h but it
	* gave me a mess of dependency errors which I wasn't able 
	* to resolve. Because only this function is needed, the best thing 
	* to do was simply doing a copy of the function - SeventySixx
	*/

	// returns false if an error occurred (either permission denied or disk full)
	bool Create(const wxString& mcdFile, uint sizeInMB)
	{
		//uint sizeInMB = 8;
		u8 m_effeffs[528 * 16];
		memset8<0xff>(m_effeffs);

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
			std::string msg  = "Memory card created: ";
			std::string name = memcard_file.GetName().ToStdString();
			msg.append(name);
			RetroMessager::Notification(msg.c_str());
			return true;
		}
		return false;
	}
}
