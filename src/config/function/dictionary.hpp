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
		const std::vector<enum ConfigType> *arg_types);

template <typename T>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(T *args),
		const std::vector<enum ConfigType> *arg_types) {
	ConfigFnDict_define_fn(dict, ident, (ConfigFn_routine)routine, Config_VOID, arg_types);
}

template <typename T, typename R>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		R *(*routine)(T *args),
		const std::vector<enum ConfigType> *arg_types) {
	enum ConfigType ret_type;
	if (typeid(R) == typeid(int32_t) || typeid(R) == typeid(uint32_t)) {
		ret_type = Config_I32;
	} else if (typeid(R) == typeid(bool)) {
		ret_type = Config_BOOL;
	} else if (typeid(R) == typeid(char *)) {
		ret_type = Config_STR;
	} else {
		LOG(Verbosity_NORMAL, "Invalid return type passed to ConfigFnDict_define_fn!\n");
		return;
	}
	// FIXME: autodetect TrackQueue * ret_type (needs TrackQueue to become opaque)

	ConfigFnDict_define_fn(dict, ident, (ConfigFn_routine)routine, ret_type, arg_types);
}

template <typename T, typename R>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		R *(*routine)(T *args),
		const enum ConfigType ret_type,
		const std::vector<enum ConfigType> *arg_types) {
	ConfigFnDict_define_fn(dict, ident, (ConfigFn_routine)routine, ret_type, arg_types);
}


// Register a config macro so it can be automatically parsed and called
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void *(*routine)(void *args),
		const enum ConfigType ret_type,
		const std::vector<enum ConfigType> *arg_types);

template <typename T>
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void (*routine)(T *args),
		const std::vector<enum ConfigType> *arg_types) {
	ConfigFnDict_define_macro(dict, ident, (ConfigFn_routine)routine, Config_VOID, arg_types);
}

template <typename T, typename R>
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		R *(*routine)(T *args),
		const std::vector<enum ConfigType> *arg_types) {
	enum ConfigType ret_type;
	if (typeid(R) == typeid(int32_t) || typeid(R) == typeid(uint32_t)) {
		ret_type = Config_I32;
	} else if (typeid(R) == typeid(bool)) {
		ret_type = Config_BOOL;
	} else if (typeid(R) == typeid(char *)) {
		ret_type = Config_STR;
	} else {
		LOG(Verbosity_NORMAL, "Invalid return type passed to ConfigFnDict_define_macro!\n");
		return;
	}

	ConfigFnDict_define_macro(dict, ident, (ConfigFn_routine)routine, ret_type, arg_types);
}
