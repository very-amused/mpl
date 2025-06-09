#include "keycode.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

const wchar_t parse_keycode(const char *keyname) {
	switch (keyname[0]) {
	case 'E':
		if (strcmp(keyname, "Escape") == 0) {
			return (wchar_t)27;
		}
		break;
	}

	// Take the first widechar as the keycode
	const size_t keyname_len = strlen(keyname);
	wchar_t keyname_wide[keyname_len];
	// Convert multibyte string to widechar string
	setlocale(LC_ALL, ""); // Define 'multibyte string' as UTF-8
	mbstowcs(keyname_wide, keyname, keyname_len);

	return keyname_wide[0];
}
