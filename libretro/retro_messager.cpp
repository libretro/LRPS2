#include "retro_messager.h"
#include <string>

namespace RetroMessager {
	void Message(
		unsigned priority, enum retro_log_level level,
		enum retro_message_target target, enum retro_message_type type,
		const char* str)
	{
		if (libretro_msg_interface_version >= 1)
		{
			struct retro_message_ext msg = {
			   str,
			   3000,
			   priority,
			   level,
			   target,
			   type,
			   -1
			};
			environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
		}
		else
		{
			struct retro_message msg =
			{
			   str,
			   180
			};
			environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
		}
	}


	void Notification(const char* str, bool logging)
	{

		if (libretro_msg_interface_version >= 1)
		{
			struct retro_message_ext msg = {
			   str,
			   3000,
			   3,
			   RETRO_LOG_INFO,
			   RETRO_MESSAGE_TARGET_OSD,
			   RETRO_MESSAGE_TYPE_NOTIFICATION,
			   -1
			};
			environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
		}
		else
		{
			struct retro_message msg =
			{
			   str,
			   180
			};
			environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
		}


	}

	void Notification(const char* str)
	{
		if (libretro_msg_interface_version >= 1)
		{
			struct retro_message_ext msg = {
			   str,
			   3000,
			   3,
			   RETRO_LOG_INFO,
			   RETRO_MESSAGE_TARGET_OSD,
			   RETRO_MESSAGE_TYPE_NOTIFICATION,
			   -1
			};
			environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
		}
		else
		{
			struct retro_message msg =
			{
			   str,
			   180
			};
			environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
		}
	}

}