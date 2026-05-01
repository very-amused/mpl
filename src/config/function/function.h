#pragma once
#include "config/function/dictionary.h"
#include "error.h"
#include "util/strtokn.h"
#include "config/parse_v2/types.h"

#include <stdbool.h>

typedef void (*ConfigFn_routine)(void *args);
typedef enum ConfigFn_ERR (*ConfigFn_argparse)(void **args, StrtoknState *parse_state);
typedef void (*ConfigFn_argfree)(void *args);

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

	// Argument parser for the function
	ConfigFn_argparse parse_args;
	// Argument deleter (deinit + free) for the function
	ConfigFn_argfree free_args;

#ifdef __cplusplus
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

// Perform a config function call
void ConfigFnCall_exec(ConfigFnCall *fn_call);

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
