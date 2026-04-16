#pragma once

#include "config/function/dictionary.h"
#include "util/strtokn.h"
#include <stdint.h>

/* Argument parsing */

enum ConfigFn_ERR argparse_noArgs(void ** args, StrtoknState *parse_state);
struct seekArgs { int32_t ms; }; // Args passed to a seek function
enum ConfigFn_ERR argparse_seekArgs(struct seekArgs **args, StrtoknState *parse_state);

/* These functions can be bound to keys and called in mpl.conf.
 * They control MPL's behavior. */

void play_toggle(void * _); // Play/pause

void quit(void * _); // Exit MPL

void seek(const struct seekArgs *args); // Seek += args.ms milliseconds
void seek_snap(const struct seekArgs *args); // Seek that snaps to the next whole second

void show_metadata(void *); // Show metadata for the current track
