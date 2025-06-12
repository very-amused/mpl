#pragma once

#include "keybind/keybind_map.h"

typedef struct mplConfig {
	KeybindMap *keybinds;
} mplConfig;

// Find a valid path to parse mpl.conf from. Allocates and sets *path and *path_len.
// Returns 0 if a path for mpl.conf was found, -1 if mpl.conf was not found, >0 for any other error.
//
// NOTE: *path is allocated using [malloc], caller is responsible for freeing
int mplConfig_find_path(char **path, size_t *path_len);

// Parse mpl.conf at *path
int mplConfig_parse(mplConfig *conf, const char *path);
// Deinitialize a parsed mpl.conf state
void mplConfig_deinit(mplConfig *conf);
