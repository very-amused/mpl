#include "cli_args.h"

#include <string.h>

struct Args args;

void args_init() {
	args.verbosity = Verbosity_NORMAL;
}

void args_parse(const int argc, const char **argv) {
	args_init();

	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const size_t arg_len = strlen(arg);

		if (arg[0] != '-' || arg_len < 2) {
			continue;
		}

		switch (arg[1]) {
		case 'v':
			args.verbosity = arg_len > 2 ? Verbosity_DEBUG : Verbosity_VERBOSE;
			break;
		// Respect `--` delimiter to stop parsing CLI args
		case '-':
			return;
		}
	}

}
