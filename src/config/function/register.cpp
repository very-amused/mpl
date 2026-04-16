#include "register.h"
extern "C" {
#include "function_definitions.h"
#include "macro_definitions.h"

#include <stdlib.h>
}

void register_ConfigFn_functions(ConfigFnDict *dict) {
	ConfigFnDict_define_fn(dict, "play_toggle",
			play_toggle,
			argparse_noArgs, free);
	ConfigFnDict_define_fn(dict, "quit",
			quit,
			argparse_noArgs, free);
	ConfigFnDict_define_fn(dict, "seek",
			seek,
			argparse_seekArgs, free);
	ConfigFnDict_define_fn(dict, "seek_snap",
			seek_snap,
			argparse_seekArgs, free);
	ConfigFnDict_define_fn(dict, "show_metadata",
			show_metadata,
			argparse_noArgs, free);
}

void register_ConfigFn_macros(ConfigFnDict *dict) {
	ConfigFnDict_define_macro(dict, "include_default_keybinds",
			include_default_keybinds,
			argparse_noArgs, free);
}
