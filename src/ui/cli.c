#include "cli.h"
#include "error.h"
#include "ui/event_queue.h"
#include "ui/input_thread.h"
#include <string.h>

struct UserInterface_CLI_opts CLI_opts;

int UserInterface_CLI_init(UserInterface_CLI *ui, const UserInterface_CLI_opts *opts) {
	// Zero pointers so we can use deinit as an escape hatch
	memset(ui, 0, sizeof(UserInterface_CLI));
	if (opts) {
		memcpy(&CLI_opts, opts, sizeof(UserInterface_CLI_opts));
	} else {
		UserInterface_CLI_opts_init(&CLI_opts);
	}


	ui->evt_queue = EventQueue_new();
	if (!ui->evt_queue) {
		UserInterface_CLI_deinit(ui);
		return 1;
	}
	ui->input_thread = InputThread_new(ui->evt_queue);
	if (!ui->input_thread) {
		UserInterface_CLI_deinit(ui);
		return 1;
	}

	return 0;
}
void UserInterface_CLI_deinit(UserInterface_CLI *ui) {
	InputThread_free(ui->input_thread);
	EventQueue_free(ui->evt_queue);
}

void UserInterface_CLI_opts_init(UserInterface_CLI_opts *opts) {
	opts->verbosity = Verbosity_NORMAL;
}

void UserInterface_CLI_opts_parse(UserInterface_CLI_opts *opts, const int argc, const char **argv) {
	// Set defaults
	UserInterface_CLI_opts_init(opts);
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const size_t arg_len = strlen(arg);

		if (arg[0] != '-' || arg_len < 2) {
			continue;
		}

		switch (arg[1]) {
		case 'v':
			opts->verbosity = arg_len > 2 ? Verbosity_DEBUG : Verbosity_VERBOSE;
			break;
		// Respect `--` delimiter to stop parsing CLI args
		case '-':
			return;
		}
	}
}
