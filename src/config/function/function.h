#pragma once
#include "error.h"
#include "util/strtokn.h"
#include "state.h"

#include <stdbool.h>

// A function callable from keybinds or MPL's shell
// These are defined using [ConfigFn_define] to define them
// in a ConfigFnDict
typedef struct ConfigFn {
	// Parsed function identifier
	char *ident;
	// Whether the function is a macro
	bool is_macro;

	// The function itself
	// *state is either fnState or macroState,
	// depending on whether the function is a macro
	void (*routine)(void *state, void *args);

	// Argument parser for the function
	enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state);
	// Argument deleter (deinit + free) for the function
	void (*free_args)(void *args);
} ConfigFn;

typedef struct ConfigFnCall {
	const ConfigFn fn; // function definition
	void *args; // function args
} ConfigFnCall;

// An array of one or more ConfigFn calls that can be
// executed in a sequence.
typedef struct ConfigFnCallArray {
	size_t n;
	ConfigFnCall **fn_calls;
} ConfigFncallArray;
