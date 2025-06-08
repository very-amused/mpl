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


int KeybindMap_parse_mapping(KeybindMap *keybinds, const char *line) {
	std::stringstream linestream(line);
	
	// bind
	std::string token;
	if (!(linestream >> token) || token != "bind") {
		// Syntax error
		return 1;
	}

	// {keyname}
	std::string keyname;
	if (!(linestream >> keyname)) {
		return 1;
	}
	const wchar_t keycode = get_keycode(keyname);

	// =
	if (!(linestream >> token) || token != "=") {
		return 1;
	}

	std::string fn_ident;
	std::getline(linestream, fn_ident, '(');
	if (!linestream.good()) {
		return 1;
	}

	// TODO: Parse fn args

	
	keybinds->map[keycode] = true;

	return 0;
}

int KeybindMap_call_keybind(const KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	return exists ? 0 : 1;
}

}
