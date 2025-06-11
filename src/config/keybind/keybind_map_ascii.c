#include "config/keybind/keycode.h"
#include "error.h"
#include "keybind_map.h"
#include "util/log.h"
#include "util/strtokn.h"

#include <wctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <locale.h>

struct KeybindMap {
	bool map[255];
};

KeybindMap *KeybindMap_new() {
	KeybindMap *keybinds = malloc(sizeof(KeybindMap));
	memset(keybinds->map, false, sizeof(KeybindMap));
	return keybinds;
}

void KeybindMap_free(KeybindMap *keybinds) {
	free(keybinds);
}

enum KeybindMap_ERR KeybindMap_parse_mapping(KeybindMap *keybinds, const char *line) {
	// Ensure utf8 support
	setlocale(LC_ALL, "");

	// Split by whitespace
	const size_t line_len = strlen(line);
	size_t offset = 0, tok_len = 0;
	static const char DELIMS[] = " \n\t\r";

#define NEXT() if (strtokn(&offset, &tok_len, line, line_len, DELIMS) != 0) return KeybindMap_SYNTAX_ERR
	// bind
	NEXT();
	if (strncmp(&line[offset], "bind", tok_len) != 0) {
		return KeybindMap_SYNTAX_ERR;
	}

	// {keyname}
	NEXT();
	char *keyname = strndup(&line[offset], tok_len);
	const wchar_t keycode = parse_keycode(keyname);
	LOG(Verbosity_DEBUG, "Parsed keycode:\n\tkeyname: %s\n", keyname);
	if (iswprint(keycode)) {
		LOG(Verbosity_DEBUG, "\tkeycode: %lc\n", keycode);
	} else {
		LOG(Verbosity_DEBUG, "\tkeycode (hex): %x\n", keycode);
	}
	LOG(Verbosity_DEBUG, "\tkeycode is unsigned ASCII: %s\n", is_uascii(keycode) ? "true" : "false");
	if (!is_uascii(keycode)) {
		free(keyname);
		return KeybindMap_NON_ASCII;
	}

	// =
	NEXT();
	
	// {function} (
	if (strtokn(&offset, &tok_len, line, line_len, "(") != 0) {
		return KeybindMap_SYNTAX_ERR;
	}
	char *fn_ident = strndup(&line[offset], tok_len);

	
#undef NEXT

	keybinds->map[keycode] = true;

	// TODO: unused
	free(keyname);
	free(fn_ident);
	

	return KeybindMap_OK;
}

enum KeybindMap_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode) {
	// Ensure we're dealing with an ASCII keycode to prevent overflow
	unsigned char ascii_keycode = keycode;
	if (keycode != ascii_keycode) {
		return KeybindMap_NON_ASCII;
	}

	const bool exists = keybinds->map[ascii_keycode];
	if (!exists) {
		return KeybindMap_NOT_FOUND;
	}

	return KeybindMap_OK;
}
