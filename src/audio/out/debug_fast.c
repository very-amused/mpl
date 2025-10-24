#include <fcntl.h>
#include <stddef.h>
#include <stdbool.h>

#include "error.h"
#include "fast.h"

#include "stream.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"
#include "util/log.h"

// FAST backend context
typedef struct Ctx {
	// Event queue for communication w/ the main thread (UI)
	EventQueue *evt_queue;

	// FAST audio sink
	FastStream *stream;

	// Playback buffer for current and next audio track
	AudioBuffer *playback_buffer;
	AudioBuffer *next_buffer;

	// Configuration from mpl.conf
	const Settings *settings;
} Ctx;

/* FAST audio backend methods */
static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings);
static void deinit(void *ctx__);
static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track);
static enum AudioBackend_ERR play(void *ctx__, bool pause);
static void lock(void *ctx__);
static void unlock(void *ctx__);
static void seek(void *ctx__);

/* AudioBackend implementation using FAST */
AudioBackend AB_FAST = {
	.name = "FAST (Fake Audio Server for Testing)",

	.init = init,
	.deinit = deinit,

	.prepare = prepare,

	.play = play,

	.lock = lock,
	.unlock = unlock,
	.seek = seek,

	.ctx_size = sizeof(Ctx)
};

/* FAST callbacks. *userdata is of type *Ctx. */
// Audio data write callback
static void FastStream_write_cb_(FastStream *stream, size_t n_bytes, void *userdata);

static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings) {
	Ctx *ctx = ctx__;

	// Connect to event queue
	ctx->evt_queue = EventQueue_connect(eq, O_WRONLY | O_NONBLOCK);
	if (!ctx->evt_queue) {
		return AudioBackend_EVENT_QUEUE_ERR;
	}
	// Store config ref
	ctx->settings = settings;

	return AudioBackend_OK;
}

static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;
	if (ctx->stream) {
		FastStream_free(ctx->stream);
	}
}

static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *t) {
	Ctx *ctx = ctx__;

	// TODO: implement terminated state for FastStream
	if (ctx->stream) {
		return AudioBackend_STREAM_EXISTS;
	}

	// Set up stream
	const AudioPCM *pcm = &t->pcm;
	const FastStreamSettings settings = {
		.sample_size = AudioPCM_sample_size(pcm),
		.n_channels = pcm->n_channels,
		.sample_rate = pcm->sample_rate,

		.buffer_ms = ctx->settings->ab_buffer_ms
	};

	ctx->stream = FastStream_new(&settings);
	if (!ctx->stream) {
		return AudioBackend_STREAM_ERR;
	}

	// Set up callbacks
	FastStream_set_write_cb(ctx->stream, FastStream_write_cb_, ctx);

	// We don't need to connect the stream to anything, since it's a null sink

	// Connect and fill(?) framebuffer
	// FIXME: could this be related to the start of audio cutoff bug?
	ctx->playback_buffer = t->buffer;
	// TODO: We need a FastStream_begin_write method.
	// TODO 2: or do we ?


	return AudioBackend_OK;
}

static enum AudioBackend_ERR play(void *ctx__, bool pause) {
	Ctx *ctx = ctx__;

	if (FastStream_play(ctx->stream, !pause) != 0) {
		return AudioBackend_PLAY_ERR;
	}

	return AudioBackend_OK;
}

static void lock(void *ctx__) {
	// TODO: we have no way to isolate and lock/unlock the callback loop
}
static void unlock(void *ctx__) {
	// TODO: we have no way to isolate and lock/unlock the callback loop
}
static void seek(void *ctx__) {
	Ctx *ctx = ctx__;

	if (!(ctx->stream && ctx->playback_buffer)) {
		return;
	}

	// FIXME FIXME FIXME: cannot implement seeks without
	// 1. lock/unlock routines
	// 2. a begin_write method to *give us the max write size*
}

static void FastStream_write_cb_(FastStream *stream, size_t n_bytes, void *userdata) {
	Ctx *ctx = userdata;

	if (!(ctx->stream && ctx->playback_buffer)) {
		return;
	}

	// Copy from track buffer to transfer buffer
	void *tb;
	size_t tb_size = n_bytes;
	tb = malloc(tb_size); // TODO: replace w/ begin_write
	if (!tb) {
		LOG(Verbosity_NORMAL, "Warning: failed to populate transfer buffer\n");
		return;
	}
	tb_size = AudioBuffer_read(ctx->playback_buffer, tb, tb_size, false);
	// Copy from transfer buffer to AB buffer
	if (FastStream_write(ctx->stream, tb, tb_size) != 0) {
		LOG(Verbosity_NORMAL, "Error: FastStream_write failed in write callback\n");
	}

	// Compute number of frames read, send to main as a timecode
	const size_t n_read = ctx->playback_buffer->n_read;
	const size_t frame_size = ctx->playback_buffer->frame_size;
	const EventBody_Timecode frames_read = n_read / frame_size;
	const Event evt = {
		.event_type = mpl_TIMECODE,
		.body_size = sizeof(EventBody_Timecode),
		.body_inline = frames_read};
	// Send timecode to the main thread
	EventQueue_send(ctx->evt_queue, &evt);
	if (tb_size == 0) {
		// Notify the main thread of track end
		const Event end_evt = {
			.event_type = mpl_TRACK_END,
			.body_size = 0};
		EventQueue_send(ctx->evt_queue, &end_evt);
	}
}
