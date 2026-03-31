#include <fcntl.h>
#include <stddef.h>
#include <stdbool.h>

#include "error.h"

#include "loop.h"
#include "server.h"
#include "stream.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"
#include "util/log.h"
#include "util/minmax.h"

// FAST backend context
typedef struct Ctx {
	// Event queue for communication w/ the main thread (UI)
	EventSubQueue *evt_sq;

	// FAST server (runtime wrapper)
	FastServer *server;
	// FAST loop (callback handler)
	FastLoop *loop;
	// FAST stream (audio sink)
	FastStream *stream;

	// Playback buffer for current and next audio track
	AudioBuffer *playback_buffer;
	AudioBuffer *next_buffer;

	// Audio transfer buffer for current and next audio track
	// (FAST doesn't do rotate buffers on a queue, so we manage transfer bufs manually)
	unsigned char *playback_tb;
	size_t playback_tb_cap;
	unsigned char *next_tb;
	size_t next_tb_cap;

	// Configuration from mpl.conf
	const Settings *settings;
} Ctx;

/* FAST audio backend methods */
static enum AudioBackend_ERR init(void *ctx__, EventQueue *eq, const Settings *settings);
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

/* Transfer buffer management */
static void realloc_buf(unsigned char **buf, size_t *buf_cap, size_t newsize);

static enum AudioBackend_ERR init(void *ctx__, EventQueue *eq, const Settings *settings) {
	Ctx *ctx = ctx__;

	// Connect to event queue
	ctx->evt_sq = EventQueue_connect(eq, BACKEND_EVT_QUEUE_SIZE);

	if (!ctx->evt_sq) {
		return AudioBackend_EVENT_QUEUE_ERR;
	}
	// Store config ref
	ctx->settings = settings;

	// Initialize FAST
	ctx->server = FastServer_new();
	if (!ctx->server) {
		return AudioBackend_CONNECT_ERR;
	}
	ctx->loop = FastLoop_new(ctx->server);
	if (!ctx->loop) {
		return AudioBackend_LOOP_STALL;
	}

	return AudioBackend_OK;
}

static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;

	FastLoop_lock(ctx->loop);
	FastStream_free(ctx->stream);
	FastLoop_free(ctx->loop);
	FastServer_free(ctx->server);
	// Free transfer buffers
	free(ctx->playback_tb);
	free(ctx->next_tb);
	// Disconnect the event queue
	// (handled in EventQueue_free now)
}

static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *t) {
	Ctx *ctx = ctx__;

	// TODO: implement terminated state for FastStream
	if (ctx->stream) {
		return AudioBackend_STREAM_EXISTS;
	}

	FastLoop_lock(ctx->loop);
#define DEINIT() \
	FastLoop_unlock(ctx->loop)

	// Set up stream
	const AudioPCM *pcm = &t->buf_pcm;
	const FastStreamSettings settings = {
		.sample_size = AudioPCM_sample_size(pcm),
		.n_channels = pcm->n_channels,
		.sample_rate = pcm->sample_rate,

		.buffer_ms = ctx->settings->ab_buffer_ms
	};

	ctx->stream = FastStream_new(ctx->loop, &settings);
	if (!ctx->stream) {
		DEINIT();
		return AudioBackend_STREAM_ERR;
	}

	// Set up callbacks
	FastStream_set_write_cb(ctx->stream, FastStream_write_cb_, ctx);

#undef DEINIT
#define DEINIT() \
	FastStream_free(ctx->stream); \
	ctx->stream = NULL; \
	FastLoop_unlock(ctx->loop)

	// We don't need to connect the stream to anything, since it's a null sink

	// Connect and fill framebuffer
	ctx->playback_buffer = t->buffer;
	size_t tb_size = AudioBuffer_max_read(ctx->playback_buffer, -1, -1, false);
	if (FastStream_begin_write(ctx->stream, &tb_size) != 0) {
		DEINIT();
		return AudioBackend_FB_WRITE_ERR;
	}

	realloc_buf(&ctx->playback_tb, &ctx->playback_tb_cap, tb_size);
	unsigned char *tb = ctx->playback_tb;
	tb_size = AudioBuffer_read(ctx->playback_buffer, tb, tb_size, false);

	if (FastStream_write(ctx->stream, tb, tb_size) != 0) {
		DEINIT();
		return AudioBackend_FB_WRITE_ERR;
	}

#undef DEINIT

	FastLoop_unlock(ctx->loop);

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
	Ctx *ctx = ctx__;
	FastLoop_lock(ctx->loop);
}
static void unlock(void *ctx__) {
	Ctx *ctx = ctx__;
	FastLoop_unlock(ctx->loop);
}
static void seek(void *ctx__) {
	Ctx *ctx = ctx__;

	if (!(ctx->stream && ctx->playback_buffer)) {
		return;
	}

	// FIXME FIXME FIXME: FAST does not yet implement seeks without
}

static void FastStream_write_cb_(FastStream *stream, size_t n_bytes, void *userdata) {
	Ctx *ctx = userdata;

	if (!(ctx->stream && ctx->playback_buffer)) {
		return;
	}

	// Copy from track buffer to transfer buffer
	size_t tb_size = MIN(n_bytes, AudioBuffer_max_read(ctx->playback_buffer, -1, -1, false));
	realloc_buf(&ctx->playback_tb, &ctx->playback_tb_cap, tb_size);
	unsigned char *tb = ctx->playback_tb;
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
	EventSubQueue_send(ctx->evt_sq, &evt, false);
	if (tb_size == 0) {
		// Notify the main thread of track end
		const Event end_evt = {
			.event_type = mpl_TRACK_END,
			.body_size = 0};
		EventSubQueue_send(ctx->evt_sq, &end_evt, false);
	}
}

static void realloc_buf(unsigned char **buf, size_t *buf_cap, size_t newsize) {
	if (newsize <= *buf_cap) {
		return;
	}
	*buf_cap = MAX(newsize, 2 * (*buf_cap));
	*buf = realloc(*buf, *buf_cap);
}
