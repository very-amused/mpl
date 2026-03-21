#pragma once

#include "error.h"
#include <wchar.h>

// Keybind map for MPL, implemented in both C (unsigned ascii support) and C++ (full UTF-8 support)
typedef struct KeybindMap KeybindMap;

// Allocate and initialize a new, empty KeybindMap
KeybindMap *KeybindMap_new();
// Deinitialize and free a KeybindMap
void KeybindMap_free(KeybindMap *keybinds);

// Parse a config line declaring a keybind.
// These lines have the form `bind x = somefn(arg1, arg2)` 
// Returns 0 on success
enum Keybind_ERR KeybindMap_parse_mapping(KeybindMap *keybinds, const char *line);

// Call the keybind mapped to {keycode} if it exists.
// Returns 0 if a keybind exists and was called, 1 if the keybind doesn't exist 
enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode);
