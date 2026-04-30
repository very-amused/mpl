extern "C" {
#include "config/setting/setting.h"

#include <string.h>
}

#include "dictionary.hpp"

#include <unordered_map>
#include <string>
#include <memory>

ConfigSetting::~ConfigSetting() {
	ConfigSetting_deinit(this);
}


struct ConfigSettingDict {
	std::unordered_map<std::string, std::unique_ptr<ConfigSetting>> map;
};

extern "C" {

ConfigSettingDict *ConfigSettingDict_new() {
	return new ConfigSettingDict;
}

void ConfigSettingDict_free(ConfigSettingDict *dict) {
	delete dict;
}

void ConfigSettingDict_lookup(ConfigSettingDict *dict, const ConfigSetting **dst, const char *ident) {
	*dst = NULL;
	const bool exists = dict->map.find(ident) != dict->map.end();
	if (!exists) {
		return;
	}

	*dst = dict->map[ident].get();
}

const bool ConfigSettingDict_has(ConfigSettingDict *dict, const char *ident) {
	return dict->map.find(ident) != dict->map.end();
}

}

void ConfigSettingDict_define(ConfigSettingDict *dict, const char *ident,
		enum ConfigType type_id, size_t struct_offset) {
	std::unique_ptr<ConfigSetting> setting = std::unique_ptr<ConfigSetting>(new ConfigSetting);
	setting->ident = strdup(ident);
	setting->type = type_id;
	setting->struct_offset = struct_offset;

	dict->map[std::string(setting->ident)] = std::move(setting);
}
