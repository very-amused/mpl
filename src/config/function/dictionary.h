#pragma once
#include "error.h"
#include "util/strtokn.h"

// A dictionary of all defined (parseable) ConfigFn's
typedef struct ConfigFnDict ConfigFnDict;

// Create an empty config function dictionary, initialized with state
// Returns NULL on error
ConfigFnDict *ConfigFnDict_new();
// Deinitialize and free a config function dictionary
void ConfigFnDict_free(ConfigFnDict *dict);

// Define a config function and add it to the dictionary
void ConfigFnDict_define(ConfigFnDict *dict, const char *ident, const bool is_macro,
		void (*routine)(void *args),
		enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state),
		void (*free_args)(void *args));
// Lookup a config function in the dictionary
typedef struct ConfigFn ConfigFn;
enum ConfigFn_ERR ConfigFnDict_lookup(ConfigFnDict *dict, const ConfigFn **dst,
		const char *ident);
