#pragma once

extern "C" {
#include "dictionary.h"
}

// Register a config function so it can be automatically parsed and called
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void *(*routine)(void *args),
		const enum ConfigType ret_type,
		const enum ConfigType *arg_types, const size_t arg_count);

template <typename T>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(T *args),
		const enum ConfigType ret_type,
		const enum ConfigType *arg_types, const size_t arg_count) {
	ConfigFnDict_define_fn(dict, ident,
			(ConfigFn_routine)routine,
			ret_type,
			arg_types, arg_count);
}

template <typename T, typename R>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		R *(*routine)(T *args),
		const enum ConfigType ret_type,
		const enum ConfigType *arg_types, const size_t arg_count) {
	ConfigFnDict_define_fn(dict, ident,
			(ConfigFn_routine)routine,
			ret_type,
			arg_types, arg_count);
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
			ret_type,
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
