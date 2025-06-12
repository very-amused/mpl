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
	// Parsed function identifier
	char *ident;
	// Function ID
	enum KeybindFnID id;

	// The function itself
	KeybindFnRoutine routine;
	// Function args passed to [call];
	// allocation/deletion are handled in [KeybindFn_parse] and [KeybindFn_deinit] respectively
	void *args;
	// Function responsible for deleting (deinit + free) [args];
	// Set in [KeybindFn_parse].
	// When args owns no heap allocated data (including when `args == NULL`),
	// we use [free] as our deleter.
	void (* args_deleter)(void *args);
} KeybindFn;

// Initialize a KeybindFn, parsing from `parse_state`;
// `parse_state` will be left with a stored token delimited the function's closing paren
enum Keybind_ERR KeybindFn_parse(KeybindFn *fn, StrtoknState *parse_state);
// Deinitialize a KeybindFn, freeing owned memory
// and calling [args_deleter(args)]
void KeybindFn_deinit(KeybindFn *fn);

// Call fn->routine with fn->args
void KeybindFn_call(const KeybindFn *fn);

// Delimiter between KeybindFn's used in mpl.conf
static const char Keybind_DELIM = ';';

// A sequence of callable KeybindFn's
typedef struct KeybindFnArray {
	size_t n;
	KeybindFn **fns;
#ifdef __cplusplus
	// Attach RAII to _parse and _deinit methods
	// Initialize to zero value
	KeybindFnArray();
	~KeybindFnArray();
#endif
} KeybindFnArray;

// Initialize a KeybindFnArray to its zero value
// NOTE: should only be used in C++ for RAII purposes. KeybindFnArray_parse is the preferred KeybindFnArray initializer.
void KeybindFnArray_init(KeybindFnArray *arr);

// Initialize a KeybindFnArray, parsing from `state_state`.
// `parse_state` will be left at the end of its string
enum Keybind_ERR KeybindFnArray_parse(KeybindFnArray *arr, StrtoknState *parse_state);

// Deinitialize a KeybindFnArray for freeing
void KeybindFnArray_deinit(KeybindFnArray *arr);
