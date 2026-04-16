#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "dictionary.h"
#include "util/strtokn.h"
#include "function.h"
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// Register a config function so it can be automatically parsed and called
// NOTE: implemented in dictionary.cpp
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(void *args),
		enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state),
		void (*free_args)(void *args));
template <typename T>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(const T *args),
		enum ConfigFn_ERR (*parse_args)(T **args, StrtoknState *parse_state),
		void (*free_args)(T *args)) {
	ConfigFnDict_define_fn(dict, ident,
			(ConfigFn_routine)routine,
			(ConfigFn_argparse)parse_args, (ConfigFn_argfree)free_args);
}
template <typename T>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(const T *args),
		enum ConfigFn_ERR (*parse_args)(T **args, StrtoknState *parse_state),
		void (*free_args)(void *args)) {
	ConfigFnDict_define_fn(dict, ident,
			(ConfigFn_routine)routine,
			(ConfigFn_argparse)parse_args, (ConfigFn_argfree)free_args);
}

// Register a config macro so it can be automatically parsed and called
// NOTE: implemented in dictionary.cpp
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void (*routine)(void *args),
		enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state),
		void (*free_args)(void *args));
template <typename T>
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void (*routine)(const T *args),
		enum ConfigFn_ERR (*parse_args)(T **args, StrtoknState *parse_state),
		void (*free_args)(T *args)) {
	ConfigFnDict_define_macro(dict, ident,
			(ConfigFn_routine)routine,
			(ConfigFn_argparse)parse_args, (ConfigFn_argfree)free_args);
}
template <typename T>
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void (*routine)(const T *args),
		enum ConfigFn_ERR (*parse_args)(T **args, StrtoknState *parse_state),
		void (*free_args)(void *args)) {
	ConfigFnDict_define_macro(dict, ident,
			(ConfigFn_routine)routine,
			(ConfigFn_argparse)parse_args, (ConfigFn_argfree)free_args);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Register defined functions with a ConfigFnDict.
 * This is the ONLY step needed to make config functions automatically parseable and callable */
void register_ConfigFn_functions(ConfigFnDict *dict);
/* Register defined macros with a ConfigFnDict.
 * This is the ONLY step needed to make config macros automatically parseable and callable */
void register_ConfigFn_macros(ConfigFnDict *dict);

#ifdef __cplusplus
}
#endif
