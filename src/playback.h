#pragma once
#include <stddef.h>
#include <stdint.h>

// A musician or group of musicians.
typedef struct Artist Artist;
// A song, piece, or tune by an Artist.
typedef struct Track Track;
// An LP, EP, or single with 1 or more Tracks.
typedef struct Album Album;

// Traditional album types
enum AlbumType {
	Unknown,
	Single,
	EP,
	LP
};

// Current track playback context
typedef struct Playback Playback;

struct Artist {
	size_t name_len;
	char *name;
};

struct Track {
	size_t name_len;
	char *name;

	uint32_t queue_no; // Queue relative
	uint32_t track_no; // Album relative

	// Primary artist
	Artist *artist;
	// All artists (including primary artist)
	size_t artists_len;
	Artist **artists;

	Album *album;
};

struct Album {
	size_t name_len;
	char *name;

	Artist *artist;

	enum AlbumType type;

	int32_t year;
};

// Current playback context
typedef struct Playback {
	Track *cur_track;
} Playback;
