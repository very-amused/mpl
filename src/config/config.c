#include "config.h"
#include "error.h"
#include "keybind/keybind_map.h"
#include "util/log.h"
#include "util/path.h"
#include "util/strtokn.h"

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

// Parse a line of mpl.conf
// Returns 0 on success, nonzero on error
// Writes error message into *strerr on error
static int mplConfig_parse_line(mplConfig *conf, const char *line,
		char *strerr, const size_t strerr_len);

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

#ifdef __unix__
// included via default_config.s
extern const char mpl_default_config[];
extern uint64_t mpl_default_config_len;
#endif

static const char *get_default_config() {
#ifdef __unix__
	return mpl_default_config;
#else
	// TODO: WIN32
	return NULL;
#endif
}

static const size_t get_default_config_len() {
#ifdef __unix__
	return mpl_default_config_len;
#else
	// TODO: WIN32
	return NULL;
#endif
}

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

int mplConfig_parse(mplConfig *conf, const char *path) {
	// Error message buffer
	char strerr[50];

	// Initialize config (zero val)
	mplConfig_init(conf);

	if (!path) {
		// Make editable copy of default config we can insert null terminators in
		const char *default_config = get_default_config();
		const size_t default_config_len = get_default_config_len();
		char *lines = strndup(default_config, default_config_len);
		// Parse default config
		StrtoknState parse_state;
		strtokn_init(&parse_state, default_config, default_config_len);
		int lineno = 1;
		while (strtokn(&parse_state, "\n") != -1) {
			// Parse line
			lines[parse_state.offset + parse_state.tok_len] = '\0';
			const char *line = &lines[parse_state.offset];
			if (mplConfig_parse_line(conf, line, strerr, sizeof(strerr)) != 0) {
				LOG(Verbosity_NORMAL, "Error parsing config at line %d: %s\n", lineno, strerr);
			}
			lineno++;
		}
		free(lines);
		return 0;
	}

	// Open file
	FILE *fp = fopen(path, "r");
	if (!fp) {
		return 1;
	}

	char *line = NULL;
	size_t line_len = 0;
	int lineno = 1;
	while (getline(&line, &line_len, fp) != EOF) {
		// Parse line
		if (mplConfig_parse_line(conf, line, strerr, sizeof(strerr)) != 0) {
			LOG(Verbosity_NORMAL, "Error parsing config at line %d: %s\n", lineno, strerr);
		}
		lineno++;
	}
	free(line);

	// Close file
	fclose(fp);

	return 0;
}


static int mplConfig_parse_line(mplConfig *conf, const char *line,
		char *strerr, const size_t strerr_len) {
	// Handle keybind lines
	static const char KEYBIND_PREFIX[] = "bind";
	if (strncmp(line, KEYBIND_PREFIX, sizeof(KEYBIND_PREFIX)-1) == 0) {
		enum Keybind_ERR err = KeybindMap_parse_mapping(conf->keybinds, line);
		if (err != Keybind_OK) {
			strncpy(strerr, Keybind_ERR_name(err), strerr_len);
			return 1;
		} 
		return 0;
	}

	// TODO: support more than just keybinds
	return 1;
}
