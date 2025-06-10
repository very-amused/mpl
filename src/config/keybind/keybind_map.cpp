extern "C" {
#include "keybind_map.h"
#include "config/keybind/keycode.h"
#include "config/functions.h"
#include "error.h"
#include "util/log.h"

#include <wctype.h>
#include <locale.h>
#include <string.h>
#include <stddef.h>
}

#include <unordered_map>
#include <string>
#include <sstream>

// A destructor function that deinitializes (if applicable) and frees the argument struct
// passed to a KeybindFn.
// If no deinitialization is needed (i.e there are only int paramters), this can be [free()]
typedef void (*KeybindFnArgDestructor)(void *);

struct KeybindRoutine {
	// Number of functions (defined in config/functions.h)
	// that this routine calls
	//
	// n_fns determines the length of [fns], [fn_args], and [fn_destructors]
	size_t n_fns;
	// Function pointer array
	KeybindFn *fns;
	// Function arg pointer array (we're responsible for freeing these)
	void **fn_args;
	// Function arg destructor array
	KeybindFnArgDestructor *fn_arg_destructors;
};

// Initialize an empty KeybindRoutine
void KeybindRoutine_init(KeybindRoutine *routine) {
	memset(routine, 0, sizeof(KeybindRoutine));
}

// Deintialize a KeybindRoutine, taking care of running
// the correct function argument destructors
void KeybindRoutine_deinit(KeybindRoutine *routine) {
	// Run destructors to deinitialize and free function args
	for (size_t i = 0; i < routine->n_fns; i++) {
		KeybindFnArgDestructor destructor = routine->fn_arg_destructors[i];
		void *args = routine->fn_args[i];
		destructor(args);
	}

	// Free arrays
	free(routine->fns);
	free(routine->fn_args);
	free(routine->fn_arg_destructors);
}

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

	
	keybinds->map[keycode] = true;

	return KeybindMap_OK;
}

enum KeybindMap_ERR KeybindMap_call_keybind(const KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	return exists ? KeybindMap_OK : KeybindMap_NOT_FOUND;
}

}
