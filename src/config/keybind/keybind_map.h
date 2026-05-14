#pragma once

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
enum Keybind_ERR KeybindMap_define_keybind(KeybindMap *keybinds, wchar_t keycode, const ParseNode *fn_list);

// Call the keybind mapped to {keycode} if it exists.
enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode);

// Call the keybind mapped to {keycode} if it exists and is enabled in the shell.
enum Keybind_ERR KeybindMap_call_shell_keybind(KeybindMap *keybinds, wchar_t keycode);
