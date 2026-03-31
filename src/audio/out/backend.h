#pragma once
#include "audio/track.h"
#include "audio/pcm.h"
#include "config/config.h"
#include "config/settings.h"
#include "error.h"
#include "ui/event_queue.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


#define BACKEND_APP_NAME "mpl"
#define BACKEND_EVT_QUEUE_SIZE 100

// Functions indicating success by returning an int
// return 0 on success, nonzero on error

// Audio output interface implemented by all audio backends.
// Q: *how* are we *playing* our audio
typedef struct AudioBackend {
	const char *name; // Backend name

	// Initialize the audio backend for playback.
	// The backend will open a subqueue to send events to EventQueue *eq.
	enum AudioBackend_ERR (*init)(void *ctx, EventQueue *eq, const Settings *settings);
	// Deinitialize the audio backend
	void (*deinit)(void *ctx);

#if defined(MPL_RESAMPLE) && !defined(MPL_RESAMPLE_PHONY)
	// Negotiate PCM with an AudioTrack, causing the AudioTrack to perform resampling
	// if needed for this AudioBackend to accept its PCM frames
	//
	// If resampling is needed, returns and sets dst_pcm to the resample destination format.
	// Otherwise, returns false and sets dst_pcm = src_pcm.
	bool (*negotiate_pcm)(void *ctx, AudioPCM *dst_pcm, const AudioPCM *src_pcm);
#endif

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
enum AudioBackend_ERR AudioBackend_init(AudioBackend *ab, EventQueue *eq, const Settings *settings);
// Deinitialize an AudioBackend
void AudioBackend_deinit(AudioBackend *ab);

#ifdef MPL_RESAMPLE
// Negotiate PCM with an AudioTrack, causing the AudioTrack to perform resampling
// if needed for this AudioBackend to accept its PCM frames
//
// If resampling is needed, returns and sets dst_pcm to the resample destination format.
// Otherwise, returns false and sets dst_pcm = src_pcm.
bool AudioBackend_negotiate_pcm(AudioBackend *ab, AudioPCM *dst_pcm, const AudioPCM *src_pcm);
#endif


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
