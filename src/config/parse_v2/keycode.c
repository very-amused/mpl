#include "keycode.h"
#include "error.h"
#include "util/log.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>

const wchar_t parse_keycode(const char *keyname) {
	// TODO: use libxkbcommon to parse keynames on Linux,
	// SDL to parse keynames on other platforms
	switch (keyname[0]) {
	case 'D':
		if (strcmp(keyname, "Down") == 0) {
			return (wchar_t)0x54;
		}
	case 'E':
		if (strcmp(keyname, "Escape") == 0) {
			return (wchar_t)0x1B;
		}
		break;
	case 'U':
		if (strcmp(keyname, "Up") == 0) {
			return (wchar_t)0x52;
		}
	}

	// Take the first widechar as the keycode
	const size_t keyname_len = strlen(keyname);
	wchar_t keyname_wide[keyname_len];
	// Convert multibyte string to widechar string
	mbstowcs(keyname_wide, keyname, keyname_len); // NOTE: ensure UTF-8 is enabled via setlocal(LC_ALL, "") beforehand

	return keyname_wide[0];
}
