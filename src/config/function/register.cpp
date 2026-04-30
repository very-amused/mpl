#include "register.h"
#include "config/parse_v2/types.h"
extern "C" {
#include "function_definitions.h"
#include "macro_definitions.h"

#include <stdlib.h>
}

void register_ConfigFn_functions(ConfigFnDict *dict) {
	ConfigFnDict_define_fn(dict, "play_toggle",
			play_toggle,
			Config_VOID,
			NULL, 0,
			argparse_noArgs, free);
	ConfigFnDict_define_fn(dict, "quit",
			quit,
			Config_VOID,
			NULL, 0,
			argparse_noArgs, free);

	static const enum ConfigType seekArgTypes[] = {Config_I32};
	static const size_t seekArgTypes_len = sizeof(seekArgTypes) / sizeof(seekArgTypes[0]);
	ConfigFnDict_define_fn(dict, "seek",
			seek,
			Config_VOID,
			seekArgTypes, seekArgTypes_len,
			argparse_seekArgs, free);
	ConfigFnDict_define_fn(dict, "seek_snap",
			seek_snap,
			Config_VOID,
			seekArgTypes, seekArgTypes_len,
			argparse_seekArgs, free);
	ConfigFnDict_define_fn(dict, "show_metadata",
			show_metadata,
			Config_VOID,
			NULL, 0,
			argparse_noArgs, free);
}

void register_ConfigFn_macros(ConfigFnDict *dict) {
	ConfigFnDict_define_macro(dict, "include_default_keybinds",
			include_default_keybinds,
			Config_VOID,
			NULL, 0,
			argparse_noArgs, free);
}
