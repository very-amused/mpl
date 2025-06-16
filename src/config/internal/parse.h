#pragma once
#include "config/config.h"

// Define flags that control exactly what is parsed from mpl.conf;
// This allows us to do things like parse only keybind lines to generate
// a default keybind map
typedef int configParseFlags;
static const configParseFlags
	PARSE_KEYBINDS = 1 << 0,
	PARSE_SETTINGS = 1 << 1,
	PARSE_MACROS = 1 << 2,
	PARSE_ALL = ~0;

// Parse mpl.conf at *path, or apply MPL's built-in default config if path == NULL.
//
// WARNING: Config_init(conf) and macroState_init(conf) MUST be called once before this function can be used. Failing to do so results in UB
int Config_parse_internal(Config *conf, const char *path, const configParseFlags flags);
