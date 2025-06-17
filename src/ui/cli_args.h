#pragma once
#include "error.h"

// MPL's CLI args that apply to all user interfaces
typedef struct Args {
	// Logging verbosity (default: Verbosity_NORMAL)
	enum Verbosity verbosity;
} Args;

// Global CLI args object
extern struct Args CLI_args;

// Initialize CLI args default values (called by args_parse)
void CLI_args_init();
// Parse CLI args from argv, use defaults for anything unspecified
void CLI_args_parse(const int argc, const char **argv);
