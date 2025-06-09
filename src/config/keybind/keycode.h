#pragma once
#include <wchar.h>

// Return key's keycode given a keyname
// i.e:
// "a" -> 'a'
// "Escape" -> (wchar_t)27
const wchar_t parse_keycode(const char *keyname);
