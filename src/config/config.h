#pragma once

#include "keybind/keybind_map.h"

typedef struct mplConfig {
	KeybindMap *keybinds;
} mplConfig;

// Find a valid path to parse mpl.conf from. Allocates *path using malloc, returns NULL if no config was found.
char *mplConfig_find_path();

// Initialize config zero value
void mplConfig_init(mplConfig *conf);
// Deinitialize config state
void mplConfig_deinit(mplConfig *conf);
// Parse mpl.conf at *path
// TODO: If *path == NULL, parse defaults. Use .incbin
int mplConfig_parse(mplConfig *conf, const char *path);
