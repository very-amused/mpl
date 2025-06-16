#include "config/functions.h"
#include "util/log.h"
#include <string.h>

int macro_eval(const char *line) {
	static const char eval_msg[] = "Evaluating macro '%s' in mpl.conf\n";

	switch (line[0]) {
	case 'i':
	{
		static const char INCLUDE_DEFAULT_KEYBINDS[] = "include_default_keybinds()";
		if (strncmp(line, INCLUDE_DEFAULT_KEYBINDS, sizeof(INCLUDE_DEFAULT_KEYBINDS)-1) == 0) {
			LOG(Verbosity_VERBOSE, eval_msg, INCLUDE_DEFAULT_KEYBINDS);
			include_default_keybinds(NULL);
			return 0;
		}
	}
	}

	return 1;
}
