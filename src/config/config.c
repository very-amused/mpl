#include "config.h"
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

void mplConfig_init(mplConfig *conf) {
	// Empty keybind map
	conf->keybinds = KeybindMap_new();
}
void mplConfig_deinit(mplConfig *conf) {
	KeybindMap_free(conf->keybinds);
}

int mplConfig_parse(mplConfig *conf, const char *path) {
	return mplConfig_parse_internal(conf, path, PARSE_ALL);
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


char *mplConfig_find_path() {
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

