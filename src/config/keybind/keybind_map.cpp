extern "C" {
#include "keybind_map.h"
#include "keycode.h"
#include "error.h"
#include "util/log.h"
#include "util/strtokn.h"
#include "function.h"

#include <wctype.h>
#include <locale.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
}

#include <unordered_map>
#include <string>
#include <memory>

KeybindFnArray::KeybindFnArray() {
	KeybindFnArray_init(this);
}
KeybindFnArray::~KeybindFnArray() {
	KeybindFnArray_deinit(this);
}

KeybindRoutineLegacy::KeybindRoutineLegacy() {
	KeybindRoutineLegacy_init(this);
}
KeybindRoutineLegacy::~KeybindRoutineLegacy() {
	KeybindRoutineLegacy_deinit(this);
}

extern "C" {

struct KeybindMap {
	std::unordered_map<wchar_t, std::unique_ptr<KeybindFnArray>> map;
};

KeybindMap *KeybindMap_new() {
	return new KeybindMap;
}

void KeybindMap_free(KeybindMap *keybinds) {
	delete keybinds;
}


enum Keybind_ERR KeybindMap_parse_mapping(KeybindMap *keybinds, const char *line) {
	// Ensure UTF-8 support
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

	// Construct our function array for this keybind
	std::unique_ptr<KeybindFnArray> fn_arr(new KeybindFnArray);
	enum Keybind_ERR err = KeybindFnArray_parse(fn_arr.get(), &parse_state);
	if (err != Keybind_OK) {
		return err;
	}

	// Add the routine to our keybind map
	keybinds->map[keycode] = std::move(fn_arr);

	return Keybind_OK;
}

enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	if (!exists) {
		return Keybind_NOT_FOUND;
	}

	KeybindFnArray *fn_arr = keybinds->map[keycode].get();
	for (size_t i = 0; i < fn_arr->n; i++) {
		KeybindFn_call(fn_arr->fns[i]);
	}

	return Keybind_OK;
}

}
