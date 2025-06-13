#pragma once

#include <stdint.h>

/* Functions usable in mpl.conf for keybinds */
void play_toggle(void * _); // Play/pause
void quit(void * _); // Exit MPL
struct seekArgs { int32_t ms; }; // Args passed to a seek function
void seek(const struct seekArgs *args); // Seek += args.ms milliseconds
void seek_snap(const struct seekArgs *args); // Seek that snaps to the next whole second

/* Functions usable in mpl.conf for direct evaluation. These cannot be used as an rvalue for keybinds */
void include_default_keybinds(void * _); // Apply all default keybinds at whatever point in mpl.conf this is called
