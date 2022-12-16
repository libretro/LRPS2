#include "libretro.h"
extern unsigned libretro_msg_interface_version;
extern retro_environment_t environ_cb;
extern retro_log_printf_t log_cb;

namespace RetroMessager {
	void Message(
		unsigned priority, enum retro_log_level level,
		enum retro_message_target target, enum retro_message_type type,
		const char* str);
	void Notification(const char* str, bool logging);
	void Notification(const char* str);

}
