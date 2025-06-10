#include "config.h"
#include "keybind/keybind_map.h"
#include "util/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Parse a line of mpl.conf
static int mplConfig_parse_line(mplConfig *conf, const char *line);

int mplConfig_parse(mplConfig *conf, const char *path) {
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
		if (mplConfig_parse_line(conf, line) != 0) {
			LOG(Verbosity_NORMAL, "Error parsing config at line %d\n", lineno);
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

static int mplConfig_parse_line(mplConfig *conf, const char *line) {
	// Handle keybind lines
	static const char KEYBIND_PREFIX[] = "bind";
	if (strncmp(line, KEYBIND_PREFIX, sizeof(KEYBIND_PREFIX)-1) == 0) {
		return KeybindMap_parse_mapping(conf->keybinds, line);
	}

	// TODO: support more than just keybinds
	return 1;
}
