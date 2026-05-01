#include "config.h"
#include "config/function/dictionary.h"
#include "config/function/state.h"
#include "config/settings.h"
#include "config/setting/register.h"
#include "error.h"
#include "keybind/keybind_map.h"
#include "util/log.h"
#include "util/path.h"
#include "function/register.h"
#include "config/parse_v2/lexer.h"
#include "config/parse_v2/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <assert.h>

void Config_init(Config *conf) {
	// Load default settings
	Settings_init(&conf->settings);


	// Load registered config functions and macros
	conf->fn_dict = ConfigFnDict_new();
	register_ConfigFn_functions(conf->fn_dict);
	register_ConfigFn_macros(conf->fn_dict);
	// Load registered config settings
	conf->setting_dict = ConfigSettingDict_new();
	register_ConfigSetting_settings(conf->setting_dict);
	// Expose the config itself to macros
	ConfigFn_macroState_init(conf);

	// Set up config/shell parsing
	conf->lexer = Lexer_new();
	conf->parser = Parser_new(conf->lexer, conf->fn_dict, conf->setting_dict);
	// TODO: Parse default config
	// We save this tree so macros can walk it later if they want to apply various defaults
	enum Parser_ERR err = Lexer_tokenize(conf->lexer, mpl_default_config);
	if (err != Parser_OK) {
		LOG(Verbosity_NORMAL, "Failed to tokenize default config: %s\n", Parser_ERR_name(err));
	}
	ParseLineError_Vec *errors; // default config parse errors
	conf->defaults = Parser_parse(conf->parser, &errors);
	for (size_t i = 0; i < errors->len; i++) {
		LOG(Verbosity_NORMAL, "Failed to parse default config: %s (line %zu)\n",
				Parser_ERR_name(err), errors->data[i].line);
	}
	ParseLineError_Vec_deinit(errors);
	free(errors);

	// Empty keybind map
	conf->keybinds = KeybindMap_new();
}
void Config_deinit(Config *conf) {
	KeybindMap_free(conf->keybinds);

	ParseNode_rfree(conf->defaults);
	Parser_free(conf->parser);
	Lexer_free(conf->lexer);
	ConfigFnDict_free(conf->fn_dict);
	ConfigSettingDict_free(conf->setting_dict);

	// Free heap-allocations used for Config_STR
	Settings_deinit(&conf->settings);
}

int Config_parse(Config *conf, const char *path) {
	// Initialize config state to its zero value
	Config_init(conf);

	FILE *fp = fopen(path, "r");
	char *line = NULL;
	size_t line_len;
	size_t lineno = 1;
	while (getline(&line, &line_len, fp) != EOF) {
		enum Parser_ERR err = Lexer_tokenize(conf->lexer, line);
		if (err != Parser_OK) {
			LOG(Verbosity_NORMAL, "Failed to parse config: %s (line %zu)\n", Parser_ERR_name(err), lineno);
		}
		lineno++;
	}
	free(line);
	fclose(fp);

	ParseLineError_Vec *parse_errs;
	ParseNode *tree = Parser_parse(conf->parser, &parse_errs);
	for (size_t i = 0; i < parse_errs->len; i++) {
		ParseLineError *err = &parse_errs->data[i];
		LOG(Verbosity_DEBUG, "Failed to parse config: %s (line %zu)\n", Parser_ERR_name(err->type), err->line);
	}
	ParseLineError_Vec_deinit(parse_errs);
	free(parse_errs);

	// Walk the parse tree to apply config
	enum Parser_ERR err = Parser_walk(conf->parser, conf, Parser_WALK_ALL, tree);
	if (err != Parser_OK) {
		LOG(Verbosity_VERBOSE, "Failed to walk config tree while parsing: %s\n", Parser_ERR_name(err));
	}
	// Free the parse tree now that we've walked and applied it
	ParseNode_rfree(tree);

	return 0;
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

