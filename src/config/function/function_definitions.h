#pragma once

#include <stdint.h>

/* Argument parsing */

// We pack argument structs so they can be dynamically encoded using just type info
#ifndef KNOWN_STRUCT_PADDING
#pragma pack(1)
#endif

struct seekArgs { int32_t ms; }; // Args passed to a seek function

// Turn struct packing off
#ifndef KNOWN_STRUCT_PADDING
#pragma pack()
#endif

/* These functions can be bound to keys and called in mpl.conf.
 * They control MPL's behavior. */

void play(void * _); // Play
void pause(void * _); // Pause
void play_toggle(void * _); // Play/pause (toggle)

void quit(void * _); // Exit MPL

void seek(const struct seekArgs *args); // Seek += args.ms milliseconds
void seek_snap(const struct seekArgs *args); // Seek that snaps to the next whole second

void show_metadata(void *); // Show metadata for the current track
