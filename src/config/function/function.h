#pragma once
#include "config/function/dictionary.h"
#include "error.h"
#include "util/strtokn.h"

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
	void (*routine)(void *args);

	// Argument parser for the function
	enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state);
	// Argument deleter (deinit + free) for the function
	void (*free_args)(void *args);

#ifdef __cplusplus
	ConfigFn();
	~ConfigFn();
#endif
} ConfigFn;

typedef struct ConfigFnCall {
	const ConfigFn *fn; // function definition
	void *args; // function args
} ConfigFnCall;

// Deinitialize a ConfigFn for freeing
void ConfigFn_deinit(ConfigFn *fn);

// Parse a single config function call
enum ConfigFn_ERR ConfigFnCall_parse(ConfigFnCall *fn_call, StrtoknState *parse_state, ConfigFnDict *fn_dict);
// Deinitialize a config function call, freeing any memory allocated during parsing
void ConfigFnCall_deinit(ConfigFnCall *fn_call);

// Delimiter between config function calls
static const char ConfigFn_DELIM = ';';

// An array of one or more ConfigFn calls that can be
// executed in a sequence.
typedef struct ConfigFnCallArray {
	size_t n;
	ConfigFnCall **fn_calls;
} ConfigFnCallArray;

// Parse a sequence of config function calls
enum ConfigFn_ERR ConfigFnCallArray_parse(ConfigFnCallArray *arr, StrtoknState *parse_state, ConfigFnDict *fn_dict);
// Deinitialize a sequence of config function calls
void ConfigFnCallArray_deinit(ConfigFnCallArray *arr);
