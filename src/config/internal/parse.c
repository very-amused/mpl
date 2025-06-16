#include "parse.h"
#include "config/config.h"
#include "config/internal/state.h"
#include "config/macro/macro.h"
#include "error.h"
#include "util/log.h"
#include "util/strtokn.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Parse a line of mpl.conf
//
// Returns 0 on success, nonzero on error
// Writes error message into *strerr on error
static int mplConfig_parse_line(mplConfig *conf, char *line,
		const configParseFlags flags,
		char *strerr, const size_t strerr_len) {
	// Discard comments
	char *comment = strchr(line, CONFIG_COMMENT);
	if (comment) {
		*comment = '\0';
	}
	// Ignore blank lines
	if (strlen(line) == 0) {
		return 0;
	}

	// Handle keybind lines
	static const char KEYBIND_PREFIX[] = "bind";
	if ((flags & PARSE_KEYBINDS) == PARSE_KEYBINDS &&
			strncmp(line, KEYBIND_PREFIX, sizeof(KEYBIND_PREFIX)-1) == 0) {
		enum Keybind_ERR err = KeybindMap_parse_mapping(conf->keybinds, line);
		if (err != Keybind_OK) {
			strncpy(strerr, Keybind_ERR_name(err), strerr_len);
			return 1;
		} 
		return 0;
	}

	// Handle macro lines
	if ((flags & PARSE_MACROS) == PARSE_MACROS) {
		int err = macro_eval(line);
		if (err != 0) {
			strncpy(strerr, "failed to evaluate macro", strerr_len);
			return 1;
		}
		return 0;
	}

	strncpy(strerr, "invalid syntax", strerr_len);
	return 1;
}

int mplConfig_parse_internal(mplConfig *conf, const char *path, const configParseFlags flags) {
	// Error message buffer
	static char strerr[50];
	// Line buffer
	static char linebuf[BUFSIZ];

	// Initialize config state to its zero value
	mplConfig_init(conf);
	// Initialize macro state so we can parse macros
	macroState_init(conf);

	if (!path) {
		// Parse our default config
		StrtoknState parse_state;
		strtokn_init(&parse_state, get_default_config(), get_default_config_len());

		int lineno = 1;
		while (strtokn(&parse_state, "\n") != -1) {
			// Check length and copy token to line buf
			static const size_t MAX = sizeof(linebuf) - 1;
			if (parse_state.tok_len > MAX) {
				LOG(Verbosity_NORMAL, "Error: config exceeds max line length of %zu at line %d\n", MAX, lineno);
				lineno++;
				continue;
			}
			strncpy(linebuf, &parse_state.s[parse_state.offset], parse_state.tok_len);
			linebuf[parse_state.tok_len] = '\0';

			// Parse line
			if (mplConfig_parse_line(conf, linebuf, flags, strerr, sizeof(strerr)) != 0) {
				LOG(Verbosity_NORMAL, "Error parsing config at line %d: %s\n", lineno, strerr);
			}
			lineno++;
		}
		return 0;
	} 

	// Parse config from file
	FILE *fp = fopen(path, "r");
	if (!fp) {
		return 1;
	}

	char *line = NULL;
	size_t line_len = 0;
	int lineno = 1;
	while (getline(&line, &line_len, fp) != EOF) {
		if (mplConfig_parse_line(conf, line, flags, strerr, sizeof(strerr)) != 0) {
			LOG(Verbosity_NORMAL, "Error parsing config at line %d: %s\n", lineno, strerr);
		}
		lineno++;
	}
	free(line);

	fclose(fp);
	return 0;
}
