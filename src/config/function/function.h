#pragma once
#include "config/parse_v2/types.h"

#include <stdbool.h>

typedef void *(*ConfigFn_routine)(void *args);

// A function callable from keybinds or MPL's shell
// These are defined using [ConfigFn_define] to define them
// in a ConfigFnDict
typedef struct ConfigFn {
	// Parsed function identifier
	char *ident;
	// Whether the function is a macro
	bool is_macro;

	// Number of arguments the function accepts
	size_t argc;
	// Type signatures for the function's arguments.
	enum ConfigType *arg_types;
	// The functions's return type
	enum ConfigType ret_type;

	// The function itself
	ConfigFn_routine routine;

#ifdef __cplusplus
	~ConfigFn();
#endif
} ConfigFn;

// Deinitialize a ConfigFn for freeing
void ConfigFn_deinit(ConfigFn *fn);
