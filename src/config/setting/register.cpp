#include "config/settings.h"
#include "dictionary.h"
#include "register.h"

extern "C" {

void register_ConfigSetting_settings(ConfigSettingDict *dict) {
	const Settings *def = &default_settings;

	ConfigSettingDict_define(dict, "at_buffer_ahead",
			def, &def->at_buffer_ahead);

	ConfigSettingDict_define(dict, "audio_backend",
			def, &def->audio_backend);
	ConfigSettingDict_define(dict, "ab_buffer_ms",
			def, &def->ab_buffer_ms);

	ConfigSettingDict_define(dict, "user_interface",
			def, &def->user_interface);
	ConfigSettingDict_define(dict, "ui_timecode_ms",
			def, &def->ui_timecode_ms);
}

}
