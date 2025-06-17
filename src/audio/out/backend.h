#pragma once
#include "audio/track.h"
#include "config/config.h"
#include "error.h"
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
	enum AudioBackend_ERR (*init)(void *ctx, const EventQueue *eq, const Config *conf);
	// Deinitialize the audio backend
	void (*deinit)(void *ctx);

	// Prepare for 'cold' playback of *track from a silent start.
	// This involves setting up an audio stream with the correct sample rate, format, channels, etc.
	enum AudioBackend_ERR (*prepare)(void *ctx, AudioTrack *track);
	// Queue up a track for upcoming gapless playback off the end of the current track.
	// Note that only one track can be queued *in the backend* at a time.
	enum AudioBackend_ERR (*queue)(void *ctx, AudioTrack *track);

	// Play/pause the current AudioTrack (prepared using prepare() or queue()).
	// If pause == 1, the track state is set to paused. Otherwise, the track state is set to playing.
	enum AudioBackend_ERR (*play)(void *ctx, bool pause);

	// Lock the backend communication loop,
	// preventing events from firing until it is unlocked
	void (*lock)(void *ctx);
	// Unlock the backend communication loop
	void (*unlock)(void *ctx);
	// Invalidate anything the backend has buffered and immediately start asking for new data,
	// used to implement seeks
	void (*seek)(void *ctx);

	// Private backend-specific context
	const size_t ctx_size;
	void *ctx;
} AudioBackend;


// Initialize an AudioBackend for playback
// The backend will open a nonblocking, write-only connection to EventQueue *eq.
enum AudioBackend_ERR AudioBackend_init(AudioBackend *ab, const EventQueue *eq, const Config *conf);
// Deinitialize an AudioBackend
void AudioBackend_deinit(AudioBackend *ab);

// Prepare for 'cold' playback of *track from a silent start.
// This involves setting up an audio stream with the correct sample rate, format, channels, etc.
enum AudioBackend_ERR AudioBackend_prepare(AudioBackend *ab, AudioTrack *track);

// Play/pause the current AudioTrack (prepared using prepare() or queue()).
// If pause == 1, the track state is set to paused. Otherwise, the track state is set to playing.
enum AudioBackend_ERR AudioBackend_play(AudioBackend *ab, bool pause);

// Lock the backend communication loop,
// preventing events from firing until it is unlocked
void AudioBackend_lock(AudioBackend *ab);
// Unlock the backend communication loop
void AudioBackend_unlock(AudioBackend *ab);
// Invalidate anything the backend has buffered and read new data from playback_buffer
void AudioBackend_seek(AudioBackend *ab);
