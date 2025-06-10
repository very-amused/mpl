#include "function.h"
#include "config/functions.h"

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
		void **fn_args, KeybindFnArgDestructor **destructor,
		const char *str, size_t offset) {
	switch (fn) {
	case bfn_play_toggle:
	{
		// TODO
		break;
	}
	case bfn_quit:
	{
		// TODO
		break;
	}
	case bfn_seek:
	{
		// TODO
		break;
	}
	case bfn_seek_snap:
	{
		// TODO
		break;
	}
	}
}

void KeybindRoutine_init(KeybindRoutine *routine) {
	memset(routine, 0, sizeof(KeybindRoutine));
}

void KeybindRoutine_deinit(KeybindRoutine *routine) {
	// Run destructors to deinitialize and free function args
	for (size_t i = 0; i < routine->n_fns; i++) {
		KeybindFnArgDestructor destructor = routine->fn_arg_destructors[i];
		void *args = routine->fn_args[i];
		destructor(args);
	}

	// Free arrays
	free(routine->fns);
	free(routine->fn_args);
	free(routine->fn_arg_destructors);
}
