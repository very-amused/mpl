#pragma once
#include "config/function/function.h"
#include "error.h"
#include "state.h"
#include "util/strtokn.h"

// A dictionary of all defined (parseable) ConfigFn's
typedef struct ConfigFnDict ConfigFnDict;

// Create an empty config function dictionary, initialized with state
// Returns NULL on error
ConfigFnDict *ConfigFnDict_new();
// Deinitialize and free a config function dictionary
void ConfigFnDict_free(ConfigFnDict *dict);

// Initialize state for macros to work (should be called before parsing mpl.conf)
// NOTE: this doesn't own anything so there's no deinit needed
void ConfigFnDict_init_macro_state(ConfigFnDict *dict, const struct macroState *state);
// Initialize state for non-macro functions to work (should be called before entering UI loop)
// NOTE: this doesn't own anything so there's no deinit needed
void ConfigFnDict_init_fn_state(ConfigFnDict *dict, const struct fnState *state);

// Define a config function and add it to the dictionary
void ConfigFn_define(ConfigFnDict *dict, const char *ident, const bool is_macro,
		void (*routine)(void *state, void *args),
		enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state),
		void (*free_args)(void *args));
