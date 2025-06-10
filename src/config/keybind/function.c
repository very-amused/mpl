#include "function.h"
#include "config/functions.h"
#include "error.h"
#include "strtokn.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

enum BindableFnID KeybindFn_getid(const char *ident) {
	switch (ident[0]) {
	case 'p':
		if (strcmp(ident, "play_toggle") == 0) {
			return bfn_play_toggle;
		}
		break;
	case 's':
		if (strcmp(ident, "seek") == 0) {
			return bfn_seek;
		} else if (strcmp(ident, "seek_snap") == 0) {
			return bfn_seek_snap;
		}
		break;
	case 'q':
		if (strcmp(ident, "quit") == 0) {
			return bfn_quit;
		}
		break;
	}

	return NULL;
}

KeybindFn KeybindFn_get(enum BindableFnID fn) {
	switch (fn) {
	case bfn_play_toggle:
		return play_toggle;
	case bfn_quit:
		return quit;
	case bfn_seek:
		return (KeybindFn)seek;
	case bfn_seek_snap:
		return (KeybindFn)seek_snap;
	}
	return NULL;
}

enum KeybindMap_ERR KeybindFn_parse_args(enum BindableFnID fn,
		void **fn_args, KeybindFnArgDeleter *deleter,
		const char *line, const size_t line_len,
		size_t offset, size_t tok_len) {

	// Defaults:
	*fn_args = NULL;
	*deleter = free; // free(NULL) is a noop

	switch (fn) {
	case bfn_play_toggle:
	{
		// No args
		if (strtokn(&offset, &tok_len, line, line_len, ")") == -1) {
			return KeybindMap_SYNTAX_ERR;
		} else if (tok_len != 0) {
			return KeybindMap_INVALID_ARG;
		}
		return KeybindMap_OK;
	}
	case bfn_quit:
	{
		if (strtokn(&offset, &tok_len, line, line_len, ")") == -1) {
			return KeybindMap_SYNTAX_ERR;
		} else if (tok_len != 0) {
			return KeybindMap_INVALID_ARG;
		}
		return KeybindMap_OK;
	}
	case bfn_seek:
	{
		// 1 arg: milliseconds (int32_t)
		if (strtokn(&offset, &tok_len, line, line_len, ")") == -1) {
			return KeybindMap_SYNTAX_ERR;
		}
		struct seekArgs *args = malloc(sizeof(struct seekArgs));
		char arg_str[tok_len+1];
		strncpy(arg_str, &line[offset], tok_len);
		arg_str[tok_len] = '\0';
		if (sscanf(arg_str, "%d", &args->ms) != 1) {
			free(args);
			return KeybindMap_INVALID_ARG;
		}
		*fn_args = args;
		return KeybindMap_OK;
	}
	case bfn_seek_snap:
	{
		if (strtokn(&offset, &tok_len, line, line_len, ")") == -1) {
			return KeybindMap_SYNTAX_ERR;
		}
		struct seekArgs *args = malloc(sizeof(struct seekArgs));
		char arg_str[tok_len+1];
		strncpy(arg_str, &line[offset], tok_len);
		arg_str[tok_len] = '\0';
		if (sscanf(arg_str, "%d", &args->ms) != 1) {
			free(args);
			return KeybindMap_INVALID_ARG;
		}
		*fn_args = args;
		return KeybindMap_OK;
	}
	}
}

void KeybindRoutine_init(KeybindRoutine *routine) {
	memset(routine, 0, sizeof(KeybindRoutine));
}

void KeybindRoutine_deinit(KeybindRoutine *routine) {
	// Run destructors to deinitialize and free function args
	for (size_t i = 0; i < routine->n_fns; i++) {
		KeybindFnArgDeleter del = routine->fn_arg_deleters[i];
		void *args = routine->fn_args[i];
		del(args);
	}

	// Free arrays
	free(routine->fns);
	free(routine->fn_args);
	free(routine->fn_arg_deleters);
}
