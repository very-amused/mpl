#pragma once
#include <stddef.h>

// Metadata for a track
typedef struct TrackMeta {
	// Track name
	char *name;
	size_t name_len;

	// Artist info
	char *artist;
	size_t artist_len;

	// Album info
	char *album;
	size_t album_len;
} TrackMeta;
