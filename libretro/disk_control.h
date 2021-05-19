#include <libretro.h>
#include "CDVD/CDVD.h"

namespace DiskControl
{
	static wxVector<wxString> disk_images;
	static int image_index = 0;
	static bool eject_state;

	static bool RETRO_CALLCONV set_eject_state(bool ejected)
	{
		if (eject_state == ejected)
			return false;

		eject_state = ejected;

		GetMTGS().FinishTaskInThread();
		CoreThread.Pause();

		if (ejected)
			cdvdCtrlTrayOpen();
		else
		{
			if (image_index < 0 || image_index >= (int)disk_images.size())
				g_Conf->CdvdSource = CDVD_SourceType::NoDisc;
			else
			{
				g_Conf->CurrentIso = disk_images[image_index];
				g_Conf->CdvdSource = CDVD_SourceType::Iso;
				CDVDsys_SetFile(CDVD_SourceType::Iso, g_Conf->CurrentIso);
			}
			cdvdCtrlTrayClose();
		}

		CoreThread.Resume();
		return true;
	}
	static bool RETRO_CALLCONV get_eject_state(void)
	{
		return eject_state;
	}

	static unsigned RETRO_CALLCONV get_image_index(void)
	{
		return image_index;
	}
	static bool RETRO_CALLCONV set_image_index(unsigned index)
	{
		if (eject_state)
			image_index = index;

		return eject_state;
	}
	static unsigned RETRO_CALLCONV get_num_images(void)
	{
		return disk_images.size();
	}

	static bool RETRO_CALLCONV replace_image_index(unsigned index, const struct retro_game_info* info)
	{
		if (index >= disk_images.size())
			return false;

		if (!info->path)
		{
			disk_images.erase(disk_images.begin() + index);
			if (!disk_images.size())
				image_index = -1;
			else if (image_index > (int)index)
				image_index--;
		}
		else
			disk_images[index] = info->path;

		return true;
	}

	static bool RETRO_CALLCONV add_image_index(void)
	{
		disk_images.push_back("");
		return true;
	}

	static bool RETRO_CALLCONV set_initial_image(unsigned index, const char* path)
	{
		if (index >= disk_images.size())
			index = 0;

		image_index = index;

		return true;
	}


	static bool RETRO_CALLCONV get_image_path(unsigned index, char* path, size_t len)
	{
		if (index >= disk_images.size())
			return false;

		if (disk_images[index].empty())
			return false;

		strncpy(path, disk_images[index].c_str(), len);
		return true;
	}

	static bool RETRO_CALLCONV get_image_label(unsigned index, char* label, size_t len)
	{
		if (index >= disk_images.size())
			return false;

		if (disk_images[index].empty())
			return false;

		strncpy(label, disk_images[index].c_str(), len);
		return true;
	}
}