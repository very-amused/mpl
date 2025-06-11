#pragma once
#include "util/strtokn.h"
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

// Get a KeybindFnID from a function's identifier (name)
// Returns -1 if no such KeybindFnID exists, this case must be checked by the caller
enum KeybindFnID KeybindFnID_from_ident(const char *ident);

// Callable function pointer to a KeybindFn's routine
typedef void (*KeybindFnRoutine)(void *args);

// Get the actual routine function ptr associated with a KeybindFnID
KeybindFnRoutine KeybindFnID_get_fn(const enum KeybindFnID fn_id);

// A function callable from keybinds, complete with its argument struct
// and associated argument deleter (destructor that frees).
typedef struct KeybindFn {
	// The function itself
	KeybindFnRoutine call;
	// Function args passed to [call];
	// allocated in [KeybindFn_parse] and deleted by calling [deleter(args)]
	void *args;
	// Function responsible for deleting (deinit + free) [args];
	// Set in [KeybindFn_parse].
	// When args owns no heap allocated data (including when `args == NULL`),
	// we use [free] as our deleter.
	void (* args_deleter)(void *args);
} KeybindFn;

/* #region Legacy routines */

// A function call that can be used in a keybind routine
// (these functions may later be usable in other places than just keybinds,
// but a name like ConfigFn would be too vague)
//
// (all KeybindFn's are defined in config/functions.h)
typedef void (*KeybindFnLegacy)(void *);

// A free function that deinitializes and frees the argument struct
// If no deinitialization is needed (i.e there are only int paramters), this can be [free()]
typedef void (*KeybindFnLegacyArgDeleter)(void *);

// Allocate and parse a KeybindFn's arguments starting at &str[offset]
//
// Sets *fn_args to point to the function's argument struct.
// Sets *deleter to point to the function's KeybindFnArgDeleter.
enum KeybindMap_ERR KeybindFnLegacy_parse_args(enum KeybindFnID fn,
		void **fn_args, KeybindFnLegacyArgDeleter *deleter, strtoknState *parse_state);

typedef struct KeybindRoutineLegacy {
	// Number of functions (defined in config/functions.h)
	// that this routine calls
	//
	// n_fns determines the length of [fns], [fn_args], and [fn_destructors]
	size_t n_fns;
	// Function pointer array
	KeybindFnLegacy *fns;
	// Function arg pointer array (we're responsible for freeing these)
	void **fn_args;
	// Function arg destructor array
	KeybindFnLegacyArgDeleter *fn_arg_deleters;
	// We connect RAII to the init/deinit functions
#ifdef __cplusplus
	KeybindRoutineLegacy();
	~KeybindRoutineLegacy();
#endif
} KeybindRoutineLegacy; // TODO: C++ RAII

// Initialize an empty KeybindRoutine
void KeybindRoutineLegacy_init(KeybindRoutineLegacy *routine);
// Deintialize a KeybindRoutine, taking care of running
// the correct function argument destructors
void KeybindRoutineLegacy_deinit(KeybindRoutineLegacy *routine);

// Push the nth KeybindFn to the back of the keybind routine.
// Returns -1 if we've hit the end of the line being parsed
enum KeybindMap_ERR KeybindRoutineLegacy_push(KeybindRoutineLegacy *routine, strtoknState *parse_state, size_t n);

/* #endregion */
