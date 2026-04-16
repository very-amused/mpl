#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "dictionary.h"
#ifdef __cplusplus
}
#endif

/* The below methods are implemeneted in dictionary.cpp */
#ifdef __cplusplus
// Register a config function so it can be automatically parsed and called
template <typename T>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(const T *args),
		enum ConfigFn_ERR (*parse_args)(T **args, StrtoknState *parse_state),
		void (*free_args)(T *args));
template <typename T>
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(const T *args),
		enum ConfigFn_ERR (*parse_args)(T **args, StrtoknState *parse_state),
		void (*free_args)(void *args));
void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(void *args),
		enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state),
		void (*free_args)(void *args));

// Register a config macro so it can be automatically parsed and called
template <typename T>
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
	void (*routine)(const T *args),
	enum ConfigFn_ERR (*parse_args)(T **args, StrtoknState *parse_state),
	void (*free_args)(T *args));
template <typename T>
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
	void (*routine)(const T *args),
	enum ConfigFn_ERR (*parse_args)(T **args, StrtoknState *parse_state),
	void (*free_args)(void *args));
void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
	void (*routine)(void *args),
	enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state),
	void (*free_args)(void *args));
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
