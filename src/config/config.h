#pragma once

#include "keybind/keybind_map.h"

typedef struct mplConfig {
	KeybindMap *keybinds;
} mplConfig;

// Parse mpl.conf at *path
int mplConfig_parse(mplConfig *conf, const char *path);
// Deinitialize a parsed mpl.conf state
void mplConfig_deinit(mplConfig *conf);
