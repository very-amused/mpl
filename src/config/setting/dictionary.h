#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "config/setting/setting.h"


// A dictionary of all defined Settings
typedef struct ConfigSettingDict ConfigSettingDict;

// Create an empty setting dictionary
// Returns NULL on error
ConfigSettingDict *ConfigSettingDict_new();
// Deinitialize and free a setting dictionary
void ConfigSettingDict_free(ConfigSettingDict *dict);

// Lookup a setting in the dictionary
// Sets *dst to NULL if ident does not match a valid setting
// NOTE: see register.h for how to define settings so they can be loaded
typedef struct ConfigSetting ConfigSetting;
void ConfigSettingDict_lookup(ConfigSettingDict *dict, const ConfigSetting **dst, const char *ident);

// Check if a setting identifier is defined in a ConfigSettingDict
const bool ConfigSettingDict_has(ConfigSettingDict *dict, const char *ident);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "config/settings.h"

#include <typeinfo>
#include <cstdint>

// Register a setting so it can be automatically parsed and read
void ConfigSettingDict_define(ConfigSettingDict *dict, const char *ident,
		enum ConfigType type_id, size_t struct_offset);

template <typename T>
void ConfigSettingDict_define(ConfigSettingDict *dict, const char *ident,
		const Settings *struct_base, T *struct_val) {
	// Get our ConfigType with C++ typeid magic
	enum ConfigType type_id;
	if (typeid(T) == typeid(int32_t) || typeid(T) == typeid(uint32_t)) {
		type_id = Config_I32;
	} else if (typeid(T) == typeid(_Bool)) {
		type_id = Config_BOOL;
	} else if (typeid(T) == typeid(char *)) {
		type_id = Config_STR;
	}

	// Compute our struct offset
	const size_t struct_offset = (unsigned char *)struct_val - (unsigned char *)struct_base;

	ConfigSettingDict_define(dict, ident, type_id, struct_offset);
}
#endif

