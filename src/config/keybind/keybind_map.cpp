extern "C" {
#include "keybind_map.h"
#include "keycode.h"
#include "config/functions.h"
#include "error.h"
#include "util/log.h"
#include "strtokn.h"
#include "function.h"

#include <wctype.h>
#include <locale.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
}

#include <unordered_map>
#include <string>

extern "C" {

struct KeybindMap {
	std::unordered_map<wchar_t, bool> map;
};

KeybindMap *KeybindMap_new() {
	return new KeybindMap;
}

void KeybindMap_free(KeybindMap *keybinds) {
	delete keybinds;
}


enum KeybindMap_ERR KeybindMap_parse_mapping(KeybindMap *keybinds, const char *line) {
	// Ensure UTF-8 support
	setlocale(LC_ALL, "");
	// Split by whitespace
	strtoknState parse_state = {
		.offset = 0,
		.tok_len = 0,
		.s = line,
		.s_len = strlen(line)
	};
	static const char DELIMS[] = " \n\t\r";

#define NEXT() if (strtokn_s(&parse_state, DELIMS) != 0) return KeybindMap_SYNTAX_ERR
	// bind
	NEXT();
	if (strncmp(&line[parse_state.offset], "bind", parse_state.tok_len) != 0) {
		return KeybindMap_SYNTAX_ERR;
	}

	// {keyname}
	NEXT();
	std::string keyname(&line[parse_state.offset], parse_state.tok_len);
	const wchar_t keycode = parse_keycode(keyname.c_str());
	LOG(Verbosity_DEBUG, "Parsed keycode:\n\tkeyname: %s\n", keyname.c_str());
	if (iswprint(keycode)) {
		LOG(Verbosity_DEBUG, "\tkeycode: %lc\n", keycode);
	} else {
		LOG(Verbosity_DEBUG, "\tkeycode (hex): %x\n", keycode);
	}
	LOG(Verbosity_DEBUG, "\tkeycode is unsigned ASCII: %s\n", is_uascii(keycode) ? "true" : "false");

	// =
	NEXT();
	
	// {function} (
	if (strtokn_s(&parse_state, "(") != 0) {
		return KeybindMap_SYNTAX_ERR;
	}
	std::string fn_ident(&line[parse_state.offset], parse_state.tok_len);
	enum KeybindFnID kbfn = KeybindFn_getid(fn_ident.c_str());
	if ((int)kbfn == -1) {
		return KeybindMap_INVALID_FN;
	}

	// TODO: support multifunction bindings

	
	keybinds->map[keycode] = true;

	return KeybindMap_OK;
}

enum KeybindMap_ERR KeybindMap_call_keybind(const KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	return exists ? KeybindMap_OK : KeybindMap_NOT_FOUND;
}

}
