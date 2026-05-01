#pragma once

extern "C" {
#include "dictionary.h"
#include "config/parse_v2/types.h"
#include "util/log.h"
}

#include <vector>
#include <typeinfo>

// Register a config function so it can be automatically parsed and called
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void *(*routine)(void *args),
		const enum ConfigType ret_type,
		std::vector<enum ConfigType> arg_types);

template <typename T>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(T *args),
		std::vector<enum ConfigType> arg_types) {
	ConfigFnDict_define_fn(dict, ident, (ConfigFn_routine)routine, Config_VOID, arg_types);
}

template <typename T, typename R>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		R *(*routine)(T *args),
		std::vector<enum ConfigType> arg_types) {
	enum ConfigType ret_type;
	if (typeid(R) == typeid(const int32_t) || typeid(R) == typeid(uint32_t)) {
		ret_type = Config_I32;
	} else if (typeid(R) == typeid(bool)) {
		ret_type = Config_BOOL;
	} else if (typeid(R) == typeid(char *)) {
		ret_type = Config_STR;
	} else {
		LOG(Verbosity_NORMAL, "Invalid type passed to ConfigFnDict_define_fn!\n");
		// WARN: do NOT return here, it will break safety checks (-Werror=maybe-uninitialized)
	}

	ConfigFnDict_define_fn(dict, ident, (ConfigFn_routine)routine, ret_type, arg_types);
}


// Register a config macro so it can be automatically parsed and called
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void *(*routine)(void *args),
		const enum ConfigType ret_type,
		const enum ConfigType *arg_types, const size_t arg_count);

template <typename T>
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void (*routine)(T *args),
		const enum ConfigType ret_type,
		const enum ConfigType *arg_types, const size_t arg_count) {
	ConfigFnDict_define_macro(dict, ident,
			(ConfigFn_routine)routine,
			Config_VOID,
			arg_types, arg_count);
}

template <typename T, typename R>
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		R *(*routine)(T *args),
		const enum ConfigType ret_type,
		const enum ConfigType *arg_types, const size_t arg_count) {
	ConfigFnDict_define_macro(dict, ident,
			(ConfigFn_routine)routine,
			ret_type,
			arg_types, arg_count);
}
