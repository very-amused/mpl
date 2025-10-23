#include <fcntl.h>
#include <stddef.h>

#include "error.h"
#include "fast.h"

#include "stream.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"

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

static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track) {
	Ctx *ctx = ctx__;

	// TODO
}
static enum AudioBackend_ERR play(void *ctx__, bool pause);
static void lock(void *ctx__);
static void unlock(void *ctx__);
static void seek(void *ctx__);
