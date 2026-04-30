#include "util/log.h"
extern "C" {
#include "dictionary.h"
#include "config/settings.h"

#include <stdint.h>
#include <stdbool.h>
}

#include <typeinfo>

// Register a setting so it can be automatically parsed and read
void ConfigSettingDict_define(ConfigSettingDict *dict, const char *ident,
		enum ConfigType type_id, size_t struct_offset);

template <typename T>
void ConfigSettingDict_define(ConfigSettingDict *dict, const char *ident,
		const Settings *struct_base, T *struct_val) {
	// Get our ConfigType with C++ typeid magic
	// NOTE: we need C++23 for these comparisons to be constexpr and thus evaluated at compile-time
	enum ConfigType type_id;
	if (typeid(T) == typeid(const int32_t) || typeid(T) == typeid(uint32_t)) {
		type_id = Config_I32;
	} else if (typeid(T) == typeid(bool)) {
		type_id = Config_BOOL;
	} else if (typeid(T) == typeid(char *)) {
		type_id = Config_STR;
	} else {
		LOG(Verbosity_NORMAL, "Invalid type passed to ConfigSettingDict_define!\n");
		// WARN: do NOT return here, it will break safety checks (-Werror=maybe-uninitialized)
	}

	// Compute our struct offset
	const size_t struct_offset = (unsigned char *)struct_val - (unsigned char *)struct_base;

	ConfigSettingDict_define(dict, ident, type_id, struct_offset);
}
