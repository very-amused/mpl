#pragma once
#include "audio/track.h"
#include "ui/event_queue.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define BACKEND_APP_NAME "mpl"

// Functions indicating success by returning an int
// return 0 on success, nonzero on error

// Audio output interface implemented by all audio backends.
// Q: *how* are we *playing* our audio
typedef struct AudioBackend {
	const char *name; // Backend name

	// Initialize the audio backend for playback.
	// The backend will open a nonblocking, write-only connection to EventQueue *eq.
	int (*init)(void *ctx, const EventQueue *eq);
	// Deinitialize the audio backend
	void (*deinit)(void *ctx);

	// Prepare for 'cold' playback of *track from a silent start.
	// This involves setting up an audio stream with the correct sample rate, format, channels, etc.
	int (*prepare)(void *ctx, AudioTrack *track);
	// Queue up a track for upcoming gapless playback off the end of the current track.
	// Note that only one track can be queued *in the backend* at a time.
	int (*queue)(void *ctx, AudioTrack *track);

	// Play/pause the current AudioTrack (prepared using prepare() or queue()).
	// If pause == 1, the track state is set to paused. Otherwise, the track state is set to playing.
	int (*play)(void *ctx, bool pause);

	// Private backend-specific context
	const size_t ctx_size;
	void *ctx;
} AudioBackend;


// Initialize an AudioBackend for playback
// The backend will open a nonblocking, write-only connection to EventQueue *eq.
int AudioBackend_init(AudioBackend *ab, const EventQueue *eq);
// Deinitialize an AudioBackend
void AudioBackend_deinit(AudioBackend *ab);

// Prepare for 'cold' playback of *track from a silent start.
// This involves setting up an audio stream with the correct sample rate, format, channels, etc.
int AudioBackend_prepare(AudioBackend *ab, AudioTrack *track);

// Play/pause the current AudioTrack (prepared using prepare() or queue()).
// If pause == 1, the track state is set to paused. Otherwise, the track state is set to playing.
int AudioBackend_play(AudioBackend *ab, bool pause);
