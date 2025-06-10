extern "C" {
#include "keybind_map.h"
#include "config/keybind/keycode.h"
#include "error.h"
#include "util/log.h"

#include <wctype.h>
#include <locale.h>
}

#include <unordered_map>
#include <string>
#include <sstream>

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
	std::stringstream linestream(line);
	
	// bind
	std::string token;
	if (!(linestream >> token) || token != "bind") {
		return KeybindMap_SYNTAX_ERR;
	}

	// {keyname}
	std::string keyname;
	if (!(linestream >> keyname)) {
		return KeybindMap_SYNTAX_ERR;
	}
	const wchar_t keycode = parse_keycode(keyname.c_str());
	LOG(Verbosity_DEBUG, "Parsed keycode:\n\tkeyname: %s\n", keyname.c_str());
	if (iswprint(keycode)) {
		LOG(Verbosity_DEBUG, "\tkeycode: %lc\n", keycode);
	} else {
		LOG(Verbosity_DEBUG, "\tkeycode (hex): %x\n", keycode);
	}
	LOG(Verbosity_DEBUG, "\tkeycode is unsigned ASCII: %s\n", is_uascii(keycode) ? "true" : "false");


	// =
	if (!(linestream >> token) || token != "=") {
		return KeybindMap_SYNTAX_ERR;
	}

	std::string fn_ident;
	std::getline(linestream, fn_ident, '(');
	if (!linestream.good()) {
		return KeybindMap_SYNTAX_ERR;
	}

	// TODO: Parse fn args

	
	keybinds->map[keycode] = true;

	return KeybindMap_OK;
}

enum KeybindMap_ERR KeybindMap_call_keybind(const KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	return exists ? KeybindMap_OK : KeybindMap_NOT_FOUND;
}

}
