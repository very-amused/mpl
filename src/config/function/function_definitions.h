#pragma once

#include "util/strtokn.h"
#include <stdint.h>

/* Argument parsing */

// We pack argument structs so they can be dynamically encoded using just type info
#pragma pack(1)

enum ConfigFn_ERR argparse_noArgs(void ** args, StrtoknState *parse_state);

struct seekArgs { int32_t ms; }; // Args passed to a seek function
enum ConfigFn_ERR argparse_seekArgs(struct seekArgs **args, StrtoknState *parse_state);

// Turn struct packing off
#pragma pack()

/* These functions can be bound to keys and called in mpl.conf.
 * They control MPL's behavior. */

void play_toggle(void * _); // Play/pause

void quit(void * _); // Exit MPL

void seek(const struct seekArgs *args); // Seek += args.ms milliseconds
void seek_snap(const struct seekArgs *args); // Seek that snaps to the next whole second

void show_metadata(void *); // Show metadata for the current track
