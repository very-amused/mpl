#include <fcntl.h>
#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>
#include <pipewire/context.h>
#include <pipewire/stream.h>

#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"
#include "error.h"
#include "pipewire/core.h"
#include "spa/utils/hook.h"
#include "ui/event.h"
#include "ui/event_queue.h"

// Pipewire backend context
typedef struct Ctx {
	// Event queue for communication with the main thread (UI)
	EventQueue *evt_queue;

	// Asynchronous event loop
	struct pw_thread_loop *loop;

	// PipeWire connection context
	struct pw_context *pw_ctx;
	// PipeWire connection core (functionally alike to PA's context)
	struct pw_core *pw_core;

	// Audio playback streams
	struct pw_stream *stream;
	struct pw_stream *next_stream;

	// Audio track data
	const AudioTrack *track;
	const AudioTrack *next_track;
} Ctx;

/* PipeWire AudioBackend methods */
static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq);
static void deinit(void *ctx__);
static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track);
static enum AudioBackend_ERR play(void *ctx__, bool pause);

/* PipeWire AudioBackend impl */
AudioBackend AB_Pipewire = {
	.name = "PipeWire",

	.init = init,
	.deinit = deinit,

	.prepare = prepare,

	.play = play,

	.ctx_size = sizeof(Ctx)
};

static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq) {
	Ctx *ctx = ctx__;

	// Connect to event queue
	ctx->evt_queue = EventQueue_connect(eq, O_WRONLY | O_NONBLOCK);
	if (!ctx->evt_queue) {
		return AudioBackend_EVENT_QUEUE_ERR;
	}

	// Initialize the Pipewire API
	pw_init(0, NULL);

	// Allocate the main loop, obtain lock for loop initialization
	ctx->loop = pw_thread_loop_new(NULL, NULL);
	if (!ctx->loop) {
		return AudioBackend_BAD_ALLOC;
	}
	pw_thread_loop_lock(ctx->loop);

	// Initialize connection context
	ctx->pw_ctx = pw_context_new(pw_thread_loop_get_loop(ctx->loop), NULL, 0);
	if (!ctx->pw_ctx) {
		fprintf(stderr, "Error: failed to create PipeWire connection context.\n");
		perror("PipeWire context");

		pw_thread_loop_unlock(ctx->loop);
		deinit(ctx);
		return AudioBackend_CONNECT_ERR;
	}

	// Set context state change callback so we get a signal back once we're connected to the server
	// TODO: Why is the callback a struct??


#undef DEINIT

	return 0;
}

static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;

	pw_thread_loop_destroy(ctx->loop);
}

static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track) {
	// TODO
	return -1;
}
static enum AudioBackend_ERR play(void *ctx__, bool pause) {
	// TODO
	return -1;
}
