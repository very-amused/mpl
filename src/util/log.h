#pragma once
#include "ui/cli.h"
#include <stdio.h>

#define LOG(lvl, ...) if (CLI_opts.verbosity >= lvl) fprintf(stderr, __VA_ARGS__)

// Configure libav logging facilities
void configure_av_log();
