#pragma once

#include <wchar.h>

// Keybind map for MPL, implemented in ~~Rust~~ C++ until I get meson playing nice
typedef struct KeybindMap KeybindMap;

// Allocate and initialize a new, empty KeybindMap
KeybindMap *KeybindMap_new();
// Deinitialize and free a KeybindMap
void KeybindMap_free(KeybindMap *keybinds);

// Parse a config line declaring a keybind.
// These lines have the form `bind x = somefn(arg1, arg2)` 
// Returns 0 on success
int KeybindMap_parse_mapping(KeybindMap *keybinds, const char *line);

// Call the keybind mapped to {keycode} if it exists.
// Returns 0 if a keybind exists and was called, 1 if the keybind doesn't exist 
int KeybindMap_call_keybind(const KeybindMap *keybinds, wchar_t keycode);
