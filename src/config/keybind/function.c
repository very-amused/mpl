#include "function.h"
#include "config/functions.h"
#include "error.h"
#include "util/strtokn.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

enum KeybindFnID KeybindFnID_from_ident(const char *ident) {
	switch (ident[0]) {
	case 'p':
		if (strcmp(ident, "play_toggle") == 0) {
			return kbfn_play_toggle;
		}
		break;
	case 's':
		if (strcmp(ident, "seek") == 0) {
			return kbfn_seek;
		} else if (strcmp(ident, "seek_snap") == 0) {
			return kbfn_seek_snap;
		}
		break;
	case 'q':
		if (strcmp(ident, "quit") == 0) {
			return kbfn_quit;
		}
		break;
	}

	return -1;
}

KeybindFnRoutine KeybindFnID_get_fn(const enum KeybindFnID fn_id) {
	switch (fn_id) {
	case kbfn_play_toggle:
		return play_toggle;
	case kbfn_quit:
		return quit;
	case kbfn_seek:
		return (KeybindFnRoutine)seek;
	case kbfn_seek_snap:
		return (KeybindFnRoutine)seek_snap;
	}
	return NULL;
}


/* #region Arg parsing for keybind functions */
static enum Keybind_ERR argparse_noArgs(KeybindFn *_, StrtoknState *parse_state) {
	if (strtokn(parse_state, ")") == -1) {
		return Keybind_SYNTAX_ERR;
	} else if (parse_state->tok_len > 0) {
		return Keybind_INVALID_ARG;
	}
	return Keybind_OK;
}
static enum Keybind_ERR argparse_seekArgs(KeybindFn *fn, StrtoknState *parse_state) {
	// 1 arg: milliseconds (int32_t)
	if (strtokn(parse_state, ")") == -1) {
		return Keybind_SYNTAX_ERR;
	}
	struct seekArgs *args = malloc(sizeof(struct seekArgs));
	char arg_str[parse_state->tok_len + 1];
	strncpy(arg_str, &parse_state->s[parse_state->offset], parse_state->tok_len);
	arg_str[parse_state->tok_len] = '\0';
	if (sscanf(arg_str, "%d", &args->ms) != 1) {
		free(args);
		return Keybind_INVALID_ARG;
	}
	fn->args = args; // default deleter of [free] works
	return Keybind_OK;
}
/* #endregion */


// Parse function arguments from `parse_state`
// Returns Keybind_INVALID_FN if fn->id doesn't have a parse case defined
static enum Keybind_ERR KeybindFn_parse_args(KeybindFn *fn, StrtoknState *parse_state) {
	// Defaults are fn->args = NULL and fn->args_deleter = free
	// (set earlier in KeybindFn_parse)
	switch (fn->id) {
		case kbfn_play_toggle:
		case kbfn_quit:
			return argparse_noArgs(fn, parse_state);
		case kbfn_seek:
		case kbfn_seek_snap:
			return argparse_seekArgs(fn, parse_state);
	}
	return Keybind_INVALID_FN;
}

enum Keybind_ERR KeybindFn_parse(KeybindFn *fn, StrtoknState *parse_state) {
	// Set zero state so KeybindFn_deinit can be called if anything fails
	fn->ident = NULL;
	fn->id = -1;
	fn->routine = NULL;
	fn->args = NULL;
	fn->args_deleter = free;

	// Parse function ident
	if (strtokn(parse_state, "(") == -1) {
		return Keybind_SYNTAX_ERR;
	}
	fn->ident = malloc((parse_state->tok_len + 1) * sizeof(char));
	strncpy(fn->ident, &parse_state->s[parse_state->offset], parse_state->tok_len);
	fn->ident[parse_state->tok_len] = '\0';

	// Get function ID
	fn->id = KeybindFnID_from_ident(fn->ident);
	if (fn->id == (enum KeybindFnID)-1) {
		return Keybind_INVALID_FN;
	}
	// Set routine (function ptr)
	fn->routine = KeybindFnID_get_fn(fn->id);
	if (!fn->routine) { // This shouldn't happen due to the above check, but it isn't worth risking a program crash
		return Keybind_INVALID_FN;
	}

	// Parse args (allocates and initializes fn->args and fn->args_deleter)
	return KeybindFn_parse_args(fn, parse_state);
}

void KeybindFn_deinit(KeybindFn *fn) {
	// Free identifier string
	free(fn->ident);
	// Delete args
	fn->args_deleter(fn->args);
}

void KeybindFn_call(const KeybindFn *fn) {
	fn->routine(fn->args);
}

void KeybindFnArray_init(KeybindFnArray *arr) {
	memset(arr, 0, sizeof(KeybindFnArray));
}

enum Keybind_ERR KeybindFnArray_parse(KeybindFnArray *arr, StrtoknState *parse_state) {
	// Zero members so KeybindFnArray_deinit is valid to call after a parse error
	memset(arr, 0, sizeof(KeybindFnArray));
	// Count closing parens to compute array size
	StrtoknState count_state = *parse_state;
	while (strtokn(&count_state, ")") != -1) {
		arr->n++;
	}

	// Allocate and zero array
	arr->fns = malloc(arr->n * sizeof(KeybindFn *));
	memset(arr->fns, 0, arr->n * sizeof(KeybindFn *));
	// TODO: we've gotten pretty bad about checking malloc's, a whole-codebase QC for allocation error handling is needed down the road

	static const char WHITESPACE[] = " \n\t\r";

	// Parse each KeybindFn
	for (size_t i = 0; i < arr->n; i++) {
		arr->fns[i] = malloc(sizeof(KeybindFn));
		enum Keybind_ERR err = KeybindFn_parse(arr->fns[i], parse_state);
		if (err != Keybind_OK) {
			return err;
		}
		if (i == arr->n-1) {
			break;
		}
		// Advance past function separator
		static const char DELIMS[] = {Keybind_DELIM, '\0'};
		if (strtokn(parse_state, DELIMS) != 0) {
			return Keybind_SYNTAX_ERR;
		}
		// Consume whitespace until next function start
		if (strtokn_consume(parse_state, WHITESPACE) != 0) {
			return Keybind_SYNTAX_ERR;
		}
	}

	return Keybind_OK;
}

void KeybindFnArray_deinit(KeybindFnArray *arr) {
	for (size_t i = 0; i < arr->n; i++) {
		if (arr->fns[i]) {
			KeybindFn_deinit(arr->fns[i]);
		}
		free(arr->fns[i]);
	}
	free(arr->fns);
}
