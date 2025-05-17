#pragma once
#include "ui/input_thread.h"
#include <stdint.h>
#include "error.h"

// MPL's CLI user interface. This is the default UI used
typedef struct UserInterface_CLI {
	EventQueue *evt_queue;
	InputThread *input_thread;
} UserInterface_CLI;

// Options for MPL's CLI user interface.
typedef struct UserInterface_CLI_opts {
	// Logging verbosity (default: Verbosity_NORMAL)
	enum Verbosity verbosity;
} UserInterface_CLI_opts;

// CLI options for MPL's CLI
extern struct UserInterface_CLI_opts CLI_opts;

// Initialize CLI user interface for use, copying *opts.
// If *opts is NULL, default CLI options are used.
// Returns 0 on success, nonzero on error
int UserInterface_CLI_init(UserInterface_CLI *ui, const UserInterface_CLI_opts *opts);
// Deinitialize CLI user interface for freeing
void UserInterface_CLI_deinit(UserInterface_CLI *ui);

// Initialize default values for CLI options
void UserInterface_CLI_opts_init(UserInterface_CLI_opts *opts);
// Parse CLI options from argv (defaults used for anything unspecified)
void UserInterface_CLI_opts_parse(UserInterface_CLI_opts *opts, const int argc, const char **argv);
