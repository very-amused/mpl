#pragma once

#include <stdint.h>

/* These functions can be bound to keys and called in mpl.conf.
 * They control MPL's behavior. */

void play_toggle(void * _); // Play/pause

void quit(void * _); // Exit MPL

struct seekArgs { int32_t ms; }; // Args passed to a seek function
void seek(const struct seekArgs *args); // Seek += args.ms milliseconds
void seek_snap(const struct seekArgs *args); // Seek that snaps to the next whole second

void show_metadata(void *); // Show metadata for the current track
