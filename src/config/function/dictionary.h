#pragma once
#include "error.h"
#include "util/strtokn.h"

// A dictionary of all defined (parseable) ConfigFn's
typedef struct ConfigFnDict ConfigFnDict;

// Create an empty config function dictionary
// Returns NULL on error
ConfigFnDict *ConfigFnDict_new();
// Deinitialize and free a config function dictionary
void ConfigFnDict_free(ConfigFnDict *dict);

// Lookup a config function in the dictionary
// NOTE: see register.h for how to define config functions so they can be loaded
typedef struct ConfigFn ConfigFn;
enum ConfigFn_ERR ConfigFnDict_lookup(ConfigFnDict *dict, const ConfigFn **dst,
		const char *ident);

// Check if a function identifier is defined in a ConfigFnDict
const bool ConfigFnDict_has(ConfigFnDict *dict, const char *ident);
