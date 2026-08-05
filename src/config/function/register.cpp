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
	ConfigFnDict_define_fn(dict, "cur_track",
			cur_track, Config_TRACK,
			NULL);
	static const std::vector<ConfigType> metadataArgTypes = {Config_TRACK};
	ConfigFnDict_define_fn(dict, "metadata",
			metadata, Config_TRACK_META,
			&metadataArgTypes);
	ConfigFnDict_define_fn(dict, "play",
			play, Config_VOID,
			NULL);
	ConfigFnDict_define_fn(dict, "pause",
			pause, Config_VOID,
			NULL);
	ConfigFnDict_define_fn(dict, "play_toggle",
			play_toggle, Config_VOID,
			NULL);
	static const std::vector<ConfigType> seekArgTypes = {Config_I32};
	ConfigFnDict_define_fn(dict, "seek",
			seek, Config_VOID,
			&seekArgTypes);
	ConfigFnDict_define_fn(dict, "seek_snap",
			seek_snap, Config_VOID,
			&seekArgTypes);
	ConfigFnDict_define_fn(dict, "show_metadata",
			show_metadata, Config_VOID,
			NULL);

	ConfigFnDict_define_fn(dict, "shell_open",
			shell_open, Config_VOID,
			NULL);
	ConfigFnDict_define_fn(dict, "shell_close",
			shell_close, Config_VOID,
			NULL);
	ConfigFnDict_define_fn(dict, "shell_history_prev",
		shell_history_prev, Config_VOID,
		NULL);
	ConfigFnDict_define_fn(dict, "shell_history_next",
		shell_history_next, Config_VOID,
		NULL);
	
	ConfigFnDict_define_fn(dict, "queue",
			queue, Config_TRACK_QUEUE,
			NULL);

	ConfigFnDict_define_fn(dict, "quit",
			quit, Config_VOID,
			NULL);
}

void register_ConfigFn_macros(ConfigFnDict *dict) {
	ConfigFnDict_define_macro(dict, "include_default_keybinds",
			include_default_keybinds, Config_VOID,
			NULL);
}
