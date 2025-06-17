#include "config.h"
#include "config/internal/state.h"
#include "config/settings.h"
#include "internal/parse.h"
#include "keybind/keybind_map.h"
#include "util/path.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>

void Config_init(Config *conf) {
	// Default settings
	Settings_init(&conf->settings);
	// Empty keybind map
	conf->keybinds = KeybindMap_new();
}
void Config_deinit(Config *conf) {
	KeybindMap_free(conf->keybinds);
}

int Config_parse(Config *conf, const char *path) {
	// Initialize config state to its zero value
	Config_init(conf);
	// Initialize macro state so we can parse macros
	macroState_init(conf);

	return Config_parse_internal(conf, path, PARSE_ALL);
}

// Try to open *path as readable and immediately close it, returns whether we succeeded
#ifndef MPL_TEST_CONFIG
static bool is_readable(const char *path) {
	FILE *fp = fopen(path, "r");
	if (!fp) {
		return false;
	}
	fclose(fp);
	return true;
}
#endif


char *Config_find_path() {
#ifdef MPL_TEST_CONFIG
	static const char testpath[] = "../test/mpl.conf";
	return strndup(testpath, sizeof(testpath)-1);
#else
	// temp path buffers we use to check file existence before setting *path and *path_len
	char *path;
	size_t path_len;

	// $HOME/mpl/mpl.conf
	const char *home = getenv("HOME");
	if (home) {
		const char *parts[] = {home, ".config", "mpl", "mpl.conf"};
		path_join(&path, &path_len, parts, sizeof(parts) / sizeof(parts[0]));
		if (is_readable(path)) {
			return path;
		}
		free(path);
	}

	return NULL;
#endif
}

