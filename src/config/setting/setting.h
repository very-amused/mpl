#pragma once
#include "config/parse_v2/types.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// A MPL setting defined using [ConfigSettingDict_define]
typedef struct ConfigSetting {
	// Name of the setting that can be parsed
	char *ident;
	// Type of the setting value
	enum ConfigType type;
	// Offset of the setting within a Settings struct
	size_t struct_offset;

#ifdef __cplusplus
	~ConfigSetting();
#endif
} ConfigSetting;

// Deinitialize a ConfigSetting for freeing
void ConfigSetting_deinit(ConfigSetting *s);
