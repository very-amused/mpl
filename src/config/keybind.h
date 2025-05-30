#pragma once
#include <wchar.h>
#include <stdint.h>

/* An MPL keybind */
typedef struct Keybind Keybind;

// Parse a Keybind from a config line of form
// bind {key} = {action}
Keybind *Keybind_parse(const char *line);
// There is intentionally no Keybind_free,
// as all keybinds must enter a KeybindMap that will
// be responsible for freeing them

/* MPL Keybind map */
typedef struct KeybindMap KeybindMap;

// Allocate and initialize a keybind map
KeybindMap *KeybindMap_new();
// Deinitialize and free a keybind map
void KeybindMap_free(KeybindMap *map);

// Bind a key. Takes ownership of *bind.
void KeybindMap_bind(KeybindMap *map, Keybind *bind);

// Unbind a key. Frees the keybind if it exists
void KeybindMap_unbind(KeybindMap *map, wchar_t key);

// Call the keybind for {key} if it exists.
void KeybindMap_call(KeybindMap *map, wchar_t key);
