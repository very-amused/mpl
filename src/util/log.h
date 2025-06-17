#pragma once
#include "ui/cli_args.h" // IWYU pragma: keep

#include <stdio.h> // IWYU pragma: keep

#define LOG(lvl, ...) if (CLI_args.verbosity >= lvl) fprintf(stderr, __VA_ARGS__)

// Configure libav logging facilities
void configure_av_log();
