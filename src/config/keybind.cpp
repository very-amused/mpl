#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>

extern "C" {
#include "keybind.h"
}

#include <unordered_map>
#include <memory>

struct Keybind {
	wchar_t key;
	void (*cb)(void *userdata);
	void *userdata;

	// C++ nonsense
	Keybind() = delete;
	Keybind(const Keybind &) = delete;
	Keybind operator=(const Keybind &) = delete;
	Keybind(const char *line); // Parse from a line of config

	~Keybind() {
		free(userdata);
	}
};

Keybind::Keybind(const char *line) {
	// TODO
}

// Parse and turn on RAII
extern "C" Keybind *Keybind_parse(const char *line) {
	return new Keybind(line);
}

struct KeybindMap {
	std::unordered_map<wchar_t, std::unique_ptr<Keybind>> map;

	KeybindMap() = default;
	KeybindMap(const KeybindMap &) = delete;
	KeybindMap operator=(const KeybindMap &) = delete;

	~KeybindMap() = default;
};

extern "C" {

KeybindMap *KeybindMap_new() {
	return new KeybindMap;
}
void KeybindMap_free(KeybindMap *map) {
	delete map;
}

void KeybindMap_bind(KeybindMap *map, Keybind *bind) {
	// Prevent user-after-free
	if (map->map.find(bind->key) != map->map.end() &&
			map->map[bind->key].get() == bind) {
		// FIXME: error log
		return;
	}
	map->map[bind->key] = std::unique_ptr<Keybind>(bind);
}

void KeybindMap_unbind(KeybindMap *map, wchar_t key) {
	map->map.erase(key);
}

// Call the keybind for {key} if it exists.
void KeybindMap_call(KeybindMap *map, wchar_t key) {
	if (map->map.find(key) == map->map.end()) {
		return;
	}

	Keybind *bind = map->map[key].get();
	bind->cb(bind->userdata);
}

}
