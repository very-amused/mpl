#include "function.h"
#include "keycode.h"
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
	KeybindFn *map[255]; // TODO: we should map KeybindRoutine's
};

KeybindMap *KeybindMap_new() {
	KeybindMap *keybinds = malloc(sizeof(KeybindMap));
	memset(keybinds->map, 0, sizeof(KeybindMap));
	return keybinds;
}

void KeybindMap_free(KeybindMap *keybinds) {
	for (size_t i = 0; i < sizeof(keybinds->map); i++) {
		if (keybinds->map[i]) {
			KeybindFn_deinit(keybinds->map[i]);
		}
		free(keybinds->map[i]);
	}
	free(keybinds);
}

enum Keybind_ERR KeybindMap_parse_mapping(KeybindMap *keybinds, const char *line) {
	// Ensure utf8 support
	setlocale(LC_ALL, "");

	// Split by whitespace
	StrtoknState parse_state;
	strtokn_init(&parse_state, line, strlen(line));
	static const char DELIMS[] = " \n\t\r";

#define NEXT() if (strtokn(&parse_state, DELIMS) != 0) return Keybind_SYNTAX_ERR
	// bind
	NEXT();
	if (strncmp(&line[parse_state.offset], "bind", parse_state.tok_len) != 0) {
		return Keybind_SYNTAX_ERR;
	}

	// {keyname}
	NEXT();
	char keyname[parse_state.tok_len+1];
	strncpy(keyname, &line[parse_state.offset], parse_state.tok_len);
	keyname[parse_state.tok_len] = '\0';
	// Parse keycode
	const wchar_t keycode = parse_keycode(keyname);
	LOG(Verbosity_DEBUG, "Parsed keycode:\n\tkeyname: %s\n", keyname);
	if (iswprint(keycode)) {
		LOG(Verbosity_DEBUG, "\tkeycode: %lc\n", keycode);
	} else {
		LOG(Verbosity_DEBUG, "\tkeycode (hex): %x\n", keycode);
	}
	LOG(Verbosity_DEBUG, "\tkeycode is unsigned ASCII: %s\n", is_uascii(keycode) ? "true" : "false");
	if (!is_uascii(keycode)) {
		return Keybind_NON_ASCII;
	}

	// =
	NEXT();
	
	// {function} (args...)
	KeybindFn *routine = malloc(sizeof(KeybindFn));
	enum Keybind_ERR status = KeybindFn_parse(routine, &parse_state);
	if (status != Keybind_OK) {
		KeybindFn_deinit(routine);
		free(routine);
		return status;
	}
	keybinds->map[keycode] = routine;
	
#undef NEXT

	return Keybind_OK;
}

enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode) {
	// Ensure we're dealing with an ASCII keycode to prevent overflow
	unsigned char ascii_keycode = keycode;
	if (keycode != ascii_keycode) {
		return Keybind_NON_ASCII;
	}

	const bool exists = keybinds->map[ascii_keycode];
	if (!exists) {
		return Keybind_NOT_FOUND;
	}

	return Keybind_OK;
}
