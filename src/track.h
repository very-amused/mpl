#pragma once
#include <stddef.h>

#include "audio/track.h"

typedef struct Track {
	// Track name
	size_t name_len;
	char *name;

	// Artist info
	size_t artist_len;
	char *artist;

	// Album info
	size_t album_len;
	char *album;

	// Audio info
	AudioTrack *audio;
} Track;
