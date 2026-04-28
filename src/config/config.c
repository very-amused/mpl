#include "config.h"
#include "config/function/dictionary.h"
#include "config/function/state.h"
#include "config/settings.h"
#include "internal/parse.h"
#include "keybind/keybind_map.h"
#include "util/path.h"
#include "function/register.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#ifdef MPL_PARSING_V2
#include "config/parse_v2/lexer.h"

#include <assert.h>
#endif

void Config_init(Config *conf) {
	// Default settings
	Settings_init(&conf->settings);
	// Load registered config functions and macros
	conf->fn_dict = ConfigFnDict_new();
	register_ConfigFn_functions(conf->fn_dict);
	register_ConfigFn_macros(conf->fn_dict);
	// Expose the config itself to macros
	ConfigFn_macroState_init(conf);
	// Empty keybind map
	conf->keybinds = KeybindMap_new();
}
void Config_deinit(Config *conf) {
	KeybindMap_free(conf->keybinds);
	ConfigFnDict_free(conf->fn_dict);
	conf->fn_dict = NULL;
	// Free heap-allocations used for Settings_STR
	Settings_deinit(&conf->settings);
}

int Config_parse(Config *conf, const char *path) {
	// Initialize config state to its zero value
	Config_init(conf);

#ifdef MPL_PARSING_V2
	Lexer *lex = Lexer_new();
	FILE *fp = fopen(path, "r");
	char *line = NULL;
	size_t line_len;
	while (getline(&line, &line_len, fp) != EOF) {
		int status = Lexer_tokenize(lex, line);
		assert(status == 0);
	}
	fclose(fp);
	Lexer_free(lex);
#endif

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
	static const char testpath[] = "../src/config/mpl-example.conf";
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

