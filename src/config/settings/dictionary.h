#pragma once
#include "error.h"

// A dictionary of all defined Settings
typedef struct ConfigSettingDict ConfigSettingDict;

// Create an empty setting dictionary
// Returns NULL on error
ConfigSettingDict *ConfigSettingDict_new();
// Deinitialize and free a setting dictionary
void ConfigSettingDict_free();

// Lookup a setting in the dictionary
// Sets *dst to NULL if ident does not match a valid setting
// NOTE: see register.h for how to define settings so they can be loaded
typedef struct ConfigSetting ConfigSetting;
void ConfigSettingDict_lookup(ConfigSettingDict *dict, const ConfigSetting **dst, const char *ident);

// Check if a setting identifier is defined in a ConfigSettingDict
const bool ConfigSettingDict_has(ConfigSettingDict *dict, const char *ident);
