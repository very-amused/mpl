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

KeybindRoutineLegacy::KeybindRoutineLegacy() {
	KeybindRoutineLegacy_init(this);
}
KeybindRoutineLegacy::~KeybindRoutineLegacy() {
	KeybindRoutineLegacy_deinit(this);
}

extern "C" {

struct KeybindMap {
	std::unordered_map<wchar_t, std::unique_ptr<KeybindRoutineLegacy>> map;
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

#define NEXT() if (strtokn_s(&parse_state, DELIMS) != 0) return Keybind_SYNTAX_ERR
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

	// Construct our routine for this keybind
	std::unique_ptr<KeybindRoutineLegacy> routine(new KeybindRoutineLegacy);
	// Count how many functions we're calling in this routine
	{
		strtoknState count_state = parse_state;
		while (strtokn_s(&count_state, ")") != -1) {
			routine->n_fns++;
		}
	}
	// Allocate routine arrays
	routine->fns = (KeybindFnLegacy *)malloc(routine->n_fns * sizeof(KeybindFnLegacy));
	routine->fn_args = (void **)malloc(routine->n_fns * sizeof(void *));
	routine->fn_arg_deleters = (KeybindFnLegacyArgDeleter *)malloc(routine->n_fns * sizeof(KeybindFnLegacyArgDeleter));
	// TODO: Safer KeybindRoutine initialization
	for (size_t i = 0; i < routine->n_fns; i++) {
		routine->fns[i] = NULL;
		routine->fn_args[i] = NULL;
		routine->fn_arg_deleters[i] = free;
	}
	// Parse routine
	for (size_t i = 0; i < routine->n_fns; i++) {
		enum KeybindMap_ERR err = KeybindRoutineLegacy_push(routine.get(), &parse_state, i);
		if (err != Keybind_OK) {
			return err;
		}
	}

	// Add the routine to our keybind map
	keybinds->map[keycode] = std::move(routine);

	return Keybind_OK;
}

enum KeybindMap_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	if (!exists) {
		return Keybind_NOT_FOUND;
	}

	KeybindRoutineLegacy *routine = keybinds->map[keycode].get();
	for (size_t i = 0; i < routine->n_fns; i++) {
		routine->fns[i](routine->fn_args[i]);
	}

	return Keybind_OK;
}

}
