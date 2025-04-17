#include <fcntl.h>
#include <stdint.h>
#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>
#include <pipewire/context.h>
#include <pipewire/stream.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#include <spa/pod/builder.h>

#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"
#include "error.h"
#include "pipewire/core.h"
#include "spa/param/audio/raw-utils.h"
#include "spa/param/param.h"
#include "spa/pod/pod.h"
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

	// Allocate the main loop
	ctx->loop = pw_thread_loop_new(NULL, NULL);
	if (!ctx->loop) {
		return AudioBackend_BAD_ALLOC;
	}

	// Lock the event loop for initialization
	pw_thread_loop_lock(ctx->loop);

	// Initialize connection context
	ctx->pw_ctx = pw_context_new(pw_thread_loop_get_loop(ctx->loop), NULL, 0);
	if (!ctx->pw_ctx) {
		fprintf(stderr, "Error: failed to create PipeWire connection context.\n");
		perror("PipeWire context");

		pw_thread_loop_unlock(ctx->loop);
		deinit(ctx);
		return AudioBackend_BAD_ALLOC
	}

	// Connect to the PW server
	ctx->pw_core = pw_context_connect(ctx->pw_ctx, NULL, 0);
	if (!ctx->pw_core) {
		fprintf(stderr, "Error: failed to connect to PipeWire.\n");
		perror("Pipewire context");

		pw_thread_loop_unlock(ctx->loop);
		deinit(ctx);
		return AudioBackend_CONNECT_ERR;
	}


	// Start the main event loop, opening communication between ctx->pw_core and PipeWire
	if (pw_thread_loop_start(ctx->loop) != 0) {
		pw_thread_loop_unlock(ctx->loop);
		deinit(ctx);
		return AudioBackend_LOOP_STALL;
	}

	return 0;
}

static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;

	pw_core_disconnect(ctx->pw_core);
	pw_context_destroy(ctx->pw_ctx);
	pw_thread_loop_destroy(ctx->loop);
}

static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *tr) {
	Ctx *ctx = ctx__;

	pw_thread_loop_lock(ctx->loop);

	// If a stream exists, the caller must use queue() instead
	if (ctx->stream && pw_stream_get_state(ctx->stream, NULL) != PW_STREAM_STATE_UNCONNECTED) {
		pw_thread_loop_unlock(ctx->loop);
		return AudioBackend_STREAM_EXISTS;
	}

	/* Configure stream params
	(this is way more complex than it needs to be thanks to PipeWire's reliance on the Simple Pile of Abstractions (SPA)) */
	uint8_t params_buf[1024]; // backing memory for the POD builder
	struct spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT(params_buf, sizeof(params_buf));

	struct spa_pod *stream_params[1];
	const struct spa_audio_info_raw audio_info = AudioPCM_pipewire_info(&tr->pcm);
	stream_params[0] = spa_format_audio_raw_build(&pod_builder,
			SPA_PARAM_EnumFormat, // Declare type as an SPA_PARAM holding a 1-value format enum. Yes, my head hurts too
			&audio_info);


	// TODO

	return -1;
}
static enum AudioBackend_ERR play(void *ctx__, bool pause) {
	// TODO
	return -1;
}
