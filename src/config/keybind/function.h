#pragma once
#include "error.h"

#define BINDABLE_FNS(VARIANT) \
	VARIANT(bfn_play_toggle) \
	VARIANT(bfn_quit) \
	VARIANT(bfn_seek) \
	VARIANT(bfn_seek_snap)

// We can't switch on function pointers,
// so we'll switch on an enum instead.
// Pain in the ass...
enum BindableFnID {
	BINDABLE_FNS(ENUM_VAL)
};

static const inline char *BindableFnID_name(enum BindableFnID bfn_id) {
	switch (bfn_id) {
		BINDABLE_FNS(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef BINDABLE_FNS

// A function call that can be used in a keybind routine
// (these functions may later be usable in other places than just keybinds,
// but a name like ConfigFn would be too vague)
//
// (all KeybindFn's are defined in config/functions.h)
typedef void (*KeybindFn)(void *);

// Get a KeybindFn's ID value by its identifier (name)
// Returns -1 if no KeybindFn with the given identifier exists
enum BindableFnID KeybindFn_getid(const char *ident);
// Get a KeybindFn's pointer from its ID value
// Returns NULL if the ID isn't a valid BindableFnID
KeybindFn KeybindFn_get(enum BindableFnID fn);

// A destructor function that deinitializes (if applicable) and frees the argument struct
// passed to a KeybindFn.
// If no deinitialization is needed (i.e there are only int paramters), this can be [free()]
typedef void (*KeybindFnArgDestructor)(void *);
// Allocate and parse a KeybindFn's arguments starting at &str[offset]
//
// Sets *fn_args to point to the function's argument struct.
// Sets *fn_destructor to point to the function's KeybindFnArgDestructor.
enum KeybindMap_ERR KeybindFn_parse_args(enum BindableFnID fn,
		void **fn_args, KeybindFnArgDestructor **destructor,
		const char *str, size_t offset);

typedef struct KeybindRoutine {
	// Number of functions (defined in config/functions.h)
	// that this routine calls
	//
	// n_fns determines the length of [fns], [fn_args], and [fn_destructors]
	size_t n_fns;
	// Function pointer array
	KeybindFn *fns;
	// Function arg pointer array (we're responsible for freeing these)
	void **fn_args;
	// Function arg destructor array
	KeybindFnArgDestructor *fn_arg_destructors;
} KeybindRoutine; // TODO: C++ RAII

// Initialize an empty KeybindRoutine
void KeybindRoutine_init(KeybindRoutine *routine);
// Deintialize a KeybindRoutine, taking care of running
// the correct function argument destructors
void KeybindRoutine_deinit(KeybindRoutine *routine);
