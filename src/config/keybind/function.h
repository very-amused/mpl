#pragma once
#include "error.h"

#define KEYBIND_FNS(VARIANT) \
	VARIANT(kbfn_play_toggle) \
	VARIANT(kbfn_quit) \
	VARIANT(kbfn_seek) \
	VARIANT(kbfn_seek_snap)

// We can't switch on function pointers,
// so we'll switch on an enum instead.
// Pain in the ass...
enum KeybindFnID {
	KEYBIND_FNS(ENUM_VAL)
};

static const inline char *KeybindFnID_name(enum KeybindFnID id) {
	switch (id) {
		KEYBIND_FNS(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef KEYBIND_FNS

// A function call that can be used in a keybind routine
// (these functions may later be usable in other places than just keybinds,
// but a name like ConfigFn would be too vague)
//
// (all KeybindFn's are defined in config/functions.h)
typedef void (*KeybindFn)(void *);

// Get a KeybindFn's ID value by its identifier (name)
// Returns -1 if no KeybindFn with the given identifier exists
enum KeybindFnID KeybindFn_getid(const char *ident);

// Get a KeybindFn's pointer from its ID value
// Returns NULL if the ID isn't a valid BindableFnID
KeybindFn KeybindFn_getfn(enum KeybindFnID id);

// A free function that deinitializes and frees the argument struct
// If no deinitialization is needed (i.e there are only int paramters), this can be [free()]
typedef void (*KeybindFnArgDeleter)(void *);

// Allocate and parse a KeybindFn's arguments starting at &str[offset]
//
// Sets *fn_args to point to the function's argument struct.
// Sets *deleter to point to the function's KeybindFnArgDeleter.
// FIXME: compact strtokn state into a struct
enum KeybindMap_ERR KeybindFn_parse_args(enum KeybindFnID fn,
		void **fn_args, KeybindFnArgDeleter *deleter,
		const char *line, const size_t line_len,
		const size_t offset, size_t prev_tok_len);

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
	KeybindFnArgDeleter *fn_arg_deleters;
} KeybindRoutine; // TODO: C++ RAII

// Initialize an empty KeybindRoutine
void KeybindRoutine_init(KeybindRoutine *routine);
// Deintialize a KeybindRoutine, taking care of running
// the correct function argument destructors
void KeybindRoutine_deinit(KeybindRoutine *routine);
