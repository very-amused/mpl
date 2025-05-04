#pragma once
#include <stddef.h>

#include "track_meta.h"
#include "audio/track.h"


typedef struct Track {
	// Track URL. All information we get about the track stems from this
	char *url;
	size_t url_len;

	// Track metadata
	TrackMeta meta;

	// Audio info
	AudioTrack *audio;
} Track;

// Does not allocate t->audio
Track *Track_new(const char *url, const size_t url_len);

void Track_free(Track *t);
