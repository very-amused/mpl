#include "error.h"
extern "C" {
#include "keybind_map.h"
}

#include <unordered_map>
#include <string>
#include <sstream>

extern "C" struct KeybindMap {
	std::unordered_map<wchar_t, bool> map;
};

static wchar_t get_keycode(const std::string &keyname) {
	// TODO
	return keyname[0];
}

extern "C" {

KeybindMap *KeybindMap_new() {
	return new KeybindMap;
}

void KeybindMap_free(KeybindMap *keybinds) {
	delete keybinds;
}


enum KeybindMap_ERR KeybindMap_parse_mapping(KeybindMap *keybinds, const char *line) {
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
	const wchar_t keycode = get_keycode(keyname);

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
