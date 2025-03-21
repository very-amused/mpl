#pragma once
#include "audio/track.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define BACKEND_APP_NAME "mpl"

// Functions indicating success by returning an int
// return 0 on success, nonzero on error

// Audio output interface implemented by all audio backends.
// Q: *how* are we *playing* our audio
typedef struct AudioBackend {
	const char *name; // Backend name

	// Initialize the audio backend for playback.
	int (*init)(void *ctx, const AudioPCM *pcm);
	// Deinitialize the audio backend
	void (*deinit)(void *ctx);

	// Prepare for 'cold' playback of *track from a silent start.
	// This involves setting up an audio stream with the correct sample rate, format, channels, etc.
	int (*prepare)(void *ctx, AudioTrack *track);
	// Queue up a track for upcoming gapless playback off the end of the current track.
	// Note that only one track can be queued *in the backend* at a time.
	int (*queue)(void *ctx, AudioTrack *track);

	// Play/pause the current AudioTrack (prepared using prepare() or queue()).
	// If p == 1, the track is played. If p == 0, the track is paused.
	// Returns 0/1 to indicate the track's current play/paused state, or negative on error.
	int (*play)(void *ctx, int p);

	// Private backend-specific context
	const size_t ctx_size;
	void *ctx;
} AudioBackend;


// Initialize an AudioBackend for playback
int AudioBackend_init(AudioBackend *ab, const AudioPCM *pcm);
// Deinitialize an AudioBackend
void AudioBackend_deinit(AudioBackend *ab);
