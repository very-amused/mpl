#include "function.h"

#include <string.h>
#include <stddef.h>
#include <stdlib.h>

void KeybindRoutine_init(KeybindRoutine *routine) {
	memset(routine, 0, sizeof(KeybindRoutine));
}

// Deintialize a KeybindRoutine, taking care of running
// the correct function argument destructors
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
