#pragma once
#include <stddef.h>

#include "audio/track.h"

typedef struct Track {
	// Track URL. All information we get about the track stems from this
	char *url;
	size_t url_len;

	// Track name
	char *name;
	size_t name_len;

	// Artist info
	char *artist;
	size_t artist_len;

	// Album info
	char *album;
	size_t album_len;

	// Audio info
	AudioTrack *audio;
} Track;

void Track_init(Track *t, const char *url, const size_t url_len);
void Track_deinit(Track *t);
