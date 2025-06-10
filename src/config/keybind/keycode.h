#pragma once
#include <wchar.h>
#include <stdbool.h>

// Return key's keycode given a keyname
// i.e:
// "a" -> 'a'
// "Escape" -> (wchar_t)27
const wchar_t parse_keycode(const char *keyname);

// Return whether a wchar_t is unsigned ascii
static inline bool is_uascii(const wchar_t keycode) {
	return keycode == (unsigned char)keycode;
}
