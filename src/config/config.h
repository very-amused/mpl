#pragma once
#include <stdint.h>

#include "settings.h"
#include "keybind/keybind_map.h"

static const char CONFIG_COMMENT = '#';

typedef struct Config {
	Settings settings;
	KeybindMap *keybinds;
} Config;

// Find a valid path to parse mpl.conf from. Allocates *path using malloc, returns NULL if no config was found.
char *Config_find_path();

// Initialize config zero value
void Config_init(Config *conf);
// Deinitialize config state
void Config_deinit(Config *conf);
// Parse mpl.conf at *path, or apply MPL's built-in default config if path == NULL
int Config_parse(Config *conf, const char *path);

/* The default mpl.conf (from this repo's root) is compiled directly into MPL
using the following symbols/resources: */
#ifdef __unix__
// included via default_config.s
extern const char mpl_default_config[];
extern uint64_t mpl_default_config_len;
#endif

static inline const char *get_default_config() {
#ifdef __unix__
	return mpl_default_config;
#else
	// TODO: WIN32
	return NULL;
#endif
}

static inline const size_t get_default_config_len() {
#ifdef __unix__
	return mpl_default_config_len;
#else
	// TODO: WIN32
	return NULL;
#endif
}
