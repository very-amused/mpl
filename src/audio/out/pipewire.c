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
static int init(void *ctx__, const EventQueue *eq);
static void deinit(void *ctx__);
static int prepare(void *ctx__, AudioTrack *track);
static int play(void *ctx__, bool pause);

/* PipeWire AudioBackend impl */
AudioBackend AB_Pipewire = {
	.name = "PipeWire",

	.init = init,
	.deinit = deinit,

	.prepare = prepare,

	.play = play,

	.ctx_size = sizeof(Ctx)
};

static int init(void *ctx__, const EventQueue *eq) {
	Ctx *ctx = ctx__;

	// Connect to event queue
	ctx->evt_queue = EventQueue_connect(eq, O_WRONLY | O_NONBLOCK);
	if (!ctx->evt_queue) {
		return 1;
	}

	// Zero streams and audio tracks
	ctx->stream = ctx->next_stream = NULL;
	ctx->track = ctx->next_track = NULL;

	// Initialize the Pipewire API
	pw_init(0, NULL);

	// Allocate the main loop, obtain lock for loop initialization
	ctx->loop = pw_thread_loop_new(NULL, NULL);
	if (!ctx->loop) {
		fprintf(stderr, "Error: failed to create PipeWire main loop.\n");
		return 1;
	}
	pw_thread_loop_lock(ctx->loop);

	// Deinitialize early if needed
#define DEINIT() \
	pw_thread_loop_unlock(ctx->loop); \
	pw_thread_loop_free(ctx->loop)

	// Initialize connection context
	ctx->pw_ctx = pw_context_new(pw_thread_loop_get_loop(ctx->loop), NULL, 0);
	if (!ctx->pw_ctx) {
		fprintf(stderr, "Error: failed to create PipeWire connection context.\n");
		perror("PipeWire context");
		return 1;
	}

	// Set context state change callback so we get a signal back once we're connected to the server
	// TODO: Why is the callback a struct??


#undef DEINIT

	return 0;
}

static void deinit(void *ctx__) {
	// TODO
}

static int prepare(void *ctx__, AudioTrack *track) {
	// TODO
	return 0;
}
static int play(void *ctx__, bool pause) {
	// TODO
	return 0;
}
