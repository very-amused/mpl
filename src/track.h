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
	AudioTrack audio;
} Track;

// As of v0.4.10, this DOES initialize track audio and metadata.
// This does NOT initialize track audio BUFFERING. The TrackQueue is in charge of managing that in a memory-efficient manner.
Track *Track_new(const char *url, const size_t url_len, AudioBackend *ab);

void Track_free(Track *t);
