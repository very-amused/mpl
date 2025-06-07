#pragma once

#include <wchar.h>

// Call the keybind assigned to {keycode} if defined.
// Returns 0 if the keybind was defined, 1 if no keybind exists for that key
int call_keybind(wchar_t keycode);
