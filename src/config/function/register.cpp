extern "C" {
#include "register.h"
#include "config/parse_v2/types.h"
#include "function_definitions.h"
#include "macro_definitions.h"

#include <stdlib.h>
}

#include "dictionary.hpp"

#include <vector>

void register_ConfigFn_functions(ConfigFnDict *dict) {
	ConfigFnDict_define_fn(dict, "play_toggle",
			play_toggle,
			NULL);
	ConfigFnDict_define_fn(dict, "quit",
			quit,
			NULL);

	static const std::vector<ConfigType> seekArgTypes = {Config_I32};
	ConfigFnDict_define_fn(dict, "seek",
			seek,
			&seekArgTypes);
	ConfigFnDict_define_fn(dict, "seek_snap",
			seek_snap,
			&seekArgTypes);
	ConfigFnDict_define_fn(dict, "show_metadata",
			show_metadata,
			&seekArgTypes);
}

void register_ConfigFn_macros(ConfigFnDict *dict) {
	ConfigFnDict_define_macro(dict, "include_default_keybinds",
			include_default_keybinds,
			NULL);
}
