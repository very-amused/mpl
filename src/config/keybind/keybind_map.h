#pragma once

#include "config/function/dictionary.h"
#include "error.h"
#include "config/parse_v2/parser.h"

#include <wchar.h>

// Keybind map for MPL, implemented in both C (unsigned ascii support) and C++ (full UTF-8 support)
typedef struct KeybindMap KeybindMap;

// Allocate and initialize a new, empty KeybindMap
KeybindMap *KeybindMap_new();
// Deinitialize and free a KeybindMap
void KeybindMap_free(KeybindMap *keybinds);

// Define a keybind mapping {keycode} to a parsed FnCallList
// NOTE: this creates a copy of the parse tree at *fn_call, allowing the original parse tree
// to be deleted
int KeybindMap_define_keybind(KeybindMap *keybinds, wchar_t keycode, const ParseNode *fn_call);

// Parse a config line declaring a keybind.
// These lines have the form `bind x = somefn(arg1, arg2)`
// Returns 0 on success
// TODO: REMOVE! DEPRECATED
enum Keybind_ERR KeybindMap_parse_mapping_legacy(KeybindMap *keybinds, const char *line, ConfigFnDict *fn_dict);

// Call the keybind mapped to {keycode} if it exists.
// Returns 0 if a keybind exists and was called, 1 if the keybind doesn't exist 
enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode);
