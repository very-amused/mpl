#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>
#include <pipewire/context.h>
#include <pipewire/stream.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#include <spa/pod/builder.h>
#include <pipewire/loop.h>

#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"
#include "error.h"
#include "pipewire/core.h"
#include "pipewire/keys.h"
#include "pipewire/properties.h"
#include "spa/param/audio/raw-utils.h"
#include "spa/param/param.h"
#include "spa/pod/pod.h"
#include "spa/utils/defs.h"
#include "spa/utils/dict.h"
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

/* PipeWire callbacks. */
// Audio data write callback
// In pipewire, writes are performed by dequeueing a buffer from a stream,
// filling it with n frames (n is an integer, no partial frames),
// and then requeueing it.
static void pw_stream_write_cb_(void *ctx__);
// Audio stream state change callback
static void pw_stream_state_cb_(void *ctx__, enum pw_stream_state old_state, enum pw_stream_state state, const char *errmsg);

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
		return AudioBackend_BAD_ALLOC;
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

	// Allocate stream
	ctx->stream = pw_stream_new(ctx->pw_core, BACKEND_APP_NAME, NULL);
	if (!ctx->stream) {
		pw_thread_loop_unlock(ctx->loop);
		return AudioBackend_STREAM_ERR;
	}

	// Set up callbacks
	static const struct pw_stream_events STREAM_EVENTS = {PW_VERSION_STREAM_EVENTS,
		.process = pw_stream_write_cb_,
		.state_changed = pw_stream_state_cb_};
	struct spa_hook stream_events_handle;
	pw_stream_add_listener(ctx->stream, &stream_events_handle, &STREAM_EVENTS, ctx);

	/* Configure stream params
	(this is way more complex than it needs to be thanks to PipeWire's reliance on the Simple Pile of Abstractions (SPA)) */
	uint8_t params_buf[1024]; // backing memory for the POD builder
	struct spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT(params_buf, sizeof(params_buf));

	const struct spa_pod *stream_params[1];
	const struct spa_audio_info_raw audio_info = AudioPCM_pipewire_info(&tr->pcm);
	stream_params[0] = spa_format_audio_raw_build(&pod_builder,
			SPA_PARAM_EnumFormat, // Declare type as an SPA_PARAM holding a 1-value format enum. Yes, my head hurts too
			&audio_info);

	// Connect stream
	int status = pw_stream_connect(ctx->stream, SPA_DIRECTION_OUTPUT, PW_ID_ANY,
			PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE,
			stream_params,
			sizeof(stream_params) / sizeof(stream_params[0]));
	if (status < 0) {
		pw_thread_loop_unlock(ctx->loop);
		return AudioBackend_CONNECT_ERR;
	}

	// Wait for stream connection to finish
	pw_thread_loop_wait(ctx->loop);
	if (pw_stream_get_state(ctx->stream, NULL) != PW_STREAM_STATE_PAUSED) {
		pw_thread_loop_unlock(ctx->loop);
		return AudioBackend_CONNECT_ERR;
	}

	pw_thread_loop_unlock(ctx->loop);

	return AudioBackend_OK;
}

static enum AudioBackend_ERR play(void *ctx__, bool pause) {
	Ctx *ctx = ctx__;

	pw_thread_loop_lock(ctx->loop);

	pw_stream_set_active(ctx->stream, !pause);
	pw_thread_loop_wait(ctx->loop);
	enum pw_stream_state stream_state = pw_stream_get_state(ctx->stream, NULL);
	const bool paused = stream_state == PW_STREAM_STATE_PAUSED;

	pw_thread_loop_unlock(ctx->loop);

	return (pause ^ !paused) ? AudioBackend_OK : AudioBackend_PLAY_ERR;
}

static void pw_stream_write_cb_(void *ctx__) {
	Ctx *ctx = ctx__;

	if (!(ctx->stream && ctx->track)) {
		return;
	}

	// Receive destination buffer from PW
	struct pw_buffer *pw_buf = pw_stream_dequeue_buffer(ctx->stream); // Pipewire Buffer
	struct spa_data tb = pw_buf->buffer->datas[0];
	if (!tb.data) {
		return;
	}

	// Compute max number of frames to write (n)
	const size_t frame_size = ctx->track->buffer->frame_size;
	uint32_t n_frames = tb.maxsize / frame_size;
	if (pw_buf->requested != 0 && pw_buf->requested < n_frames) {
		n_frames = pw_buf->requested;
	}
	// Read frames from track buffer
	pw_buf->size = AudioBuffer_read(ctx->track->buffer, tb.data, n_frames * frame_size) / frame_size;

	// Return transfer buffer to PW
	pw_stream_queue_buffer(ctx->stream, pw_buf);

	// Compute and send timecode to the main thread
	const size_t n_read = ctx->track->buffer->n_read;
	const EventBody_Timecode frames_read = n_read / frame_size;
	const Event evt = {
		.event_type = mpl_TIMECODE,
		.body_size = sizeof(EventBody_Timecode),
		.body_inline = frames_read};
	EventQueue_send(ctx->evt_queue, &evt);
}

static void pw_stream_state_cb_(void *ctx__, enum pw_stream_state old_state, enum pw_stream_state state, const char *errmsg) {
	Ctx *ctx = ctx__;

	switch (state) {
	case PW_STREAM_STATE_STREAMING:
	case PW_STREAM_STATE_ERROR:
	case PW_STREAM_STATE_PAUSED:
		pw_thread_loop_signal(ctx->loop, 0);
	
	case PW_STREAM_STATE_UNCONNECTED:
	case PW_STREAM_STATE_CONNECTING:
		break;
	}
}
