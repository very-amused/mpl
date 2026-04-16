#include "function.h"
#include "config/function/dictionary.h"
#include "error.h"
#include "util/strtokn.h"

#include <stdlib.h>
#include <string.h>

/* #region ConfigFn */

void ConfigFn_deinit(ConfigFn *fn) {
	free(fn->ident);
	fn->ident = NULL;
}


/* #endregion */

/* #region ConfigFnCall */

enum ConfigFn_ERR ConfigFnCall_parse(ConfigFnCall *fn_call, StrtoknState *parse_state, ConfigFnDict *fn_dict) {
	memset(fn_call, 0, sizeof(ConfigFnCall));

	// Parse function ident and lookup function
	if (strtokn(parse_state, "(") == -1) {
		return ConfigFn_SYNTAX_ERR;
	}
	char *ident = strndup(&parse_state->s[parse_state->offset], parse_state->tok_len);
	CHECK_ALLOC(ident, ConfigFn_BAD_ALLOC);
	enum ConfigFn_ERR err = ConfigFnDict_lookup(fn_dict, &fn_call->fn, ident);
	free(ident);
	if (err != ConfigFn_OK) {
		return err;
	}

	// Parse args using the function's provided arg parsing routine
	return fn_call->fn->parse_args(&fn_call->args, parse_state);
}

void ConfigFnCall_deinit(ConfigFnCall *fn_call) {
	fn_call->fn->free_args(fn_call->args);
	fn_call->args = NULL;
}

/* #endregion */

/* #region ConfigFnCallArray */

enum ConfigFn_ERR ConfigFnCallArray_parse(ConfigFnCallArray *arr, StrtoknState *parse_state, ConfigFnDict *fn_dict) {
	memset(arr, 0, sizeof(ConfigFnCallArray));

	// Count closing parents to compute array size
	StrtoknState count_state = *parse_state;
	while (strtokn(&count_state, ")") != -1) {
		arr->n++;
	}

	// Allocate and zero array
	arr->fn_calls = malloc(arr->n * sizeof(ConfigFnCall *));
	CHECK_ALLOC(arr->fn_calls, ConfigFn_BAD_ALLOC);
	memset(arr->fn_calls, 0, arr->n * sizeof(ConfigFnCall *));

	// Parse each function call
	for (size_t i = 0; i < arr->n; i++) {
		arr->fn_calls[i] = malloc(sizeof(ConfigFnCall));
		CHECK_ALLOC(arr->fn_calls[i], ConfigFn_BAD_ALLOC);
		enum ConfigFn_ERR err = ConfigFnCall_parse(arr->fn_calls[i], parse_state, fn_dict);
		if (err != ConfigFn_OK) {
			return err;
		}
		if (i == arr->n-1) {
			break;
		}
		// Advance past function separator
		static const char DELIMS[] = {ConfigFn_DELIM, '\0'};
		if (strtokn(parse_state, DELIMS) != 0) {
			return ConfigFn_SYNTAX_ERR;
		}
		// Consume whitespace until next function start
		if (strtokn_consume(parse_state, WHITESPACE) != 0) {
			return ConfigFn_SYNTAX_ERR;
		}
	}

	return ConfigFn_OK;
}

void ConfigFnCallArray_deinit(ConfigFnCallArray *arr) {
	for (size_t i = 0; i < arr->n; i++) {
		ConfigFnCall_deinit(arr->fn_calls[i]);
	}
	free(arr->fn_calls);
	arr->n = 0;
	arr->fn_calls = NULL; }

/* #endregion */
