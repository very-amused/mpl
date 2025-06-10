#pragma once
#include "error.h"

#include <stdint.h>

/* Functions usable in mpl.conf */
void play_toggle(); // Play/pause
void quit(); // Exit MPL
struct seekArgs { int32_t ms; }; // Args passed to a seek function
void seek(const struct seekArgs *args); // Seek += args.ms milliseconds
void seek_snap(const struct seekArgs *args); // Seek that snaps to the next whole second
