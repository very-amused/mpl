#pragma once
#include "ui/cli.h"

#define LOG(lvl, ...) if (CLI_opts.verbosity >= lvl) fprintf(stderr, __VA_ARGS__);
