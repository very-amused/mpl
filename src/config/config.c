#include "config.h"
#include "error.h"
#include "keybind/keybind_map.h"
#include "util/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

// Parse a line of mpl.conf
// Returns 0 on success, nonzero on error
// Writes error message into *strerr on error
static int mplConfig_parse_line(mplConfig *conf, const char *line,
		char *strerr, const size_t strerr_len);

int mplConfig_parse(mplConfig *conf, const char *path) {
	// Error message buffer
	char strerr[50];

	// Initialize config values
	conf->keybinds = KeybindMap_new();
	
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

void mplConfig_deinit(mplConfig *conf) {
	KeybindMap_free(conf->keybinds);
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
