#include "function.h"
#include "config/functions.h"
#include "error.h"
#include "util/strtokn.h"
#include "util/log.h"

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
		return (KeybindFnLegacy)seek;
	case kbfn_seek_snap:
		return (KeybindFnLegacy)seek_snap;
	}
	return NULL;
}


/* #region Arg parsing for keybind functions */
static enum KeybindMap_ERR argparse_noArgs(strtoknState *parse_state) {
	if (strtokn_s(parse_state, ")") == -1) {
		return Keybind_SYNTAX_ERR;
	} else if (parse_state->tok_len > 0) {
		return Keybind_INVALID_ARG;
	}
	return Keybind_OK;
}
static enum KeybindMap_ERR argparse_seekArgs(void **fn_args, KeybindFnLegacyArgDeleter *deleter, strtoknState *parse_state) {
	// 1 arg: milliseconds (int32_t)
	if (strtokn_s(parse_state, ")") == -1) {
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
	*fn_args = args;
	return Keybind_OK;
}
/* #endregion */

enum KeybindMap_ERR KeybindFnLegacy_parse_args(enum KeybindFnID fn,
		void **fn_args, KeybindFnLegacyArgDeleter *deleter, strtoknState *parse_state) {

	// Defaults:
	*fn_args = NULL;
	*deleter = free; // free(NULL) is a noop

	switch (fn) {
	case kbfn_play_toggle:
		return argparse_noArgs(parse_state);
	case kbfn_quit:
		return argparse_noArgs(parse_state);
	case kbfn_seek:
		return argparse_seekArgs(fn_args, deleter, parse_state);
	case kbfn_seek_snap:
		return argparse_seekArgs(fn_args, deleter, parse_state);
	}

	return Keybind_NOT_FOUND;
}


void KeybindRoutineLegacy_init(KeybindRoutineLegacy *routine) {
	memset(routine, 0, sizeof(KeybindRoutineLegacy));
}

void KeybindRoutineLegacy_deinit(KeybindRoutineLegacy *routine) {
	// Run destructors to deinitialize and free function args
	for (size_t i = 0; i < routine->n_fns; i++) {
		KeybindFnLegacyArgDeleter del = routine->fn_arg_deleters[i];
		void *args = routine->fn_args[i];
		del(args);
	}

	// Free arrays
	free(routine->fns);
	free(routine->fn_args);
	free(routine->fn_arg_deleters);
}

enum KeybindMap_ERR KeybindRoutineLegacy_push(KeybindRoutineLegacy *routine, strtoknState *parse_state, size_t n) {
	// Parse function ident
	// {function} (
	if (strtokn_s(parse_state, "(") != 0) {
		return Keybind_SYNTAX_ERR;
	}
	char fn_ident[parse_state->tok_len + 1];
	strncpy(fn_ident, &parse_state->s[parse_state->offset], parse_state->tok_len);
	fn_ident[parse_state->tok_len] = '\0';
	LOG(Verbosity_DEBUG, "fn_ident: %s\n", fn_ident);

	// Get function ID from ident
	enum KeybindFnID fn_id = KeybindFnID_from_ident(fn_ident);
	if ((int)fn_id == -1) {
		return Keybind_INVALID_FN;
	}

	routine->fns[n] = KeybindFnID_get_fn(fn_id);
	enum KeybindMap_ERR err = KeybindFnLegacy_parse_args(fn_id,
			&routine->fn_args[n], &routine->fn_arg_deleters[n], parse_state);
	if (err != Keybind_OK) {
		return err;
	}

	// Parse through the next function separator
	if (n == routine->n_fns-1) {
		return Keybind_OK;
	}
	static const char FUNCTION_DELIMS[] = ";";
	if (strtokn_s(parse_state, FUNCTION_DELIMS) == -1) {
		return Keybind_SYNTAX_ERR;
	}

	// Eat any whitespace after delim
	static const char WHITESPACE[] = " \n\t\r";

	// TODO
	// FIXME FIXME: this is insanely sketchy
	parse_state->offset += parse_state->tok_len + sizeof(char);
	// Max token length
	const size_t max_len = parse_state->s_len - parse_state->offset;
	// Token start
	const char *start = parse_state->s + parse_state->offset;
	// Reset current token length
	parse_state->tok_len = 0;
	while (parse_state->tok_len < max_len) {
		const char c = start[parse_state->tok_len];
		bool is_whitespace = false;
		for (const char *ws = &WHITESPACE[0]; *ws != '\0'; ws++) {
			if (c == *ws) {
				is_whitespace = true;
				break;
			}
		}
		if (!is_whitespace) {
			if (parse_state->tok_len > 0) {
				parse_state->offset += parse_state->tok_len;
				parse_state->tok_len = 0;
			}
			return Keybind_OK;
		}
		parse_state->tok_len++;
	}

	return Keybind_SYNTAX_ERR; // fn()) [mismatched parens]
}
