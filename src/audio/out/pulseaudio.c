#include <fcntl.h>
#include <pulse/channelmap.h>
#include <pulse/def.h>
#include <pulse/operation.h>
#include <pulse/proplist.h>
#include <pulse/sample.h>
#include <pulse/thread-mainloop.h>
#include <pulse/context.h>
#include <pulse/stream.h>
#include <pulse/error.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"
#include "config/settings.h"
#include "error.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "util/log.h"

// PulseAudio backend context
typedef struct Ctx {
	// Event queue for communicating w/ the main thread (UI)
	EventQueue *evt_queue;

	// Asynchronous event loop
	pa_threaded_mainloop *loop;

	// PA connection context
	pa_context *pa_ctx;

	// Audio playback stream
	pa_stream *stream;
	pa_stream *next_stream;
	// PA can't accept planar samples afaict, so we need to know if we need to interlace them.
	AudioPCM PCM;

	// Playback buffer for current and next audio track
	AudioBuffer *playback_buffer;
	AudioBuffer *next_buffer;

	// Configuration from mpl.conf
	const Settings *settings;
} Ctx;

/* PulseAudio AudioBackend methods */
static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings);
static void deinit(void *ctx__);
static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track);
static enum AudioBackend_ERR play(void *ctx__, bool pause);
static void lock(void *ctx__);
static void unlock(void *ctx__);
static void seek(void *ctx__);

/* AudioBackend implementation using PulseAudio */
AudioBackend AB_PulseAudio = {
	.name = "PulseAudio",

	.init = init,
	.deinit = deinit,

	.prepare = prepare,

	.play = play,

	.lock = lock,
	.unlock = unlock,
	.seek = seek,

	.ctx_size = sizeof(Ctx)
};

/* PulseAudio callbacks. *userdata is of type *Ctx. */
// Connection state change callback
static void pa_ctx_state_cb_(pa_context *pa_ctx, void *userdata);
// Audio data write callback
static void pa_stream_write_cb_(pa_stream *stream, size_t n_bytes, void *userdata);
// Audio stream state change callback
static void pa_stream_state_cb_(pa_stream *stream, void *userdata);
// Operation completion callback
static void pa_stream_success_cb_(pa_stream *stream, int success, void *userdata);

static enum AudioBackend_ERR init(void *userdata, const EventQueue *eq, const Settings *settings) {
	Ctx *ctx = userdata;

	// Connect to event queue
	ctx->evt_queue = EventQueue_connect(eq, O_WRONLY | O_NONBLOCK);
	if (!ctx->evt_queue) {
		return AudioBackend_EVENT_QUEUE_ERR;
	}
	// Store config ref
	ctx->settings = settings;

	/* Set up our PulseAudio event loop and connection objects */

	// Allocate the main event loop, obtain lock for loop initialization
	ctx->loop = pa_threaded_mainloop_new();
	if (!ctx->loop) {
		return AudioBackend_BAD_ALLOC;
	}
	pa_threaded_mainloop_lock(ctx->loop);

	// Deinitialize early if we have an error to prevent resource leaks
#define DEINIT() \
	pa_threaded_mainloop_unlock(ctx->loop); \
	pa_threaded_mainloop_free(ctx->loop)

	// Initialize connection context
	ctx->pa_ctx = pa_context_new(
			pa_threaded_mainloop_get_api(ctx->loop),
			BACKEND_APP_NAME);
	if (!ctx->pa_ctx) {
		DEINIT();
		return AudioBackend_CONNECT_ERR;
	}
	
	// Set context state change callback,
	// which will send us back a signal when we've conclusively succeeded or failed at connecting to the server
	pa_context_set_state_callback(ctx->pa_ctx, pa_ctx_state_cb_, ctx);

#undef DEINIT
#define DEINIT() \
	pa_context_disconnect(ctx->pa_ctx); \
	pa_context_unref(ctx->pa_ctx); \
	pa_threaded_mainloop_unlock(ctx->loop); \
	pa_threaded_mainloop_free(ctx->loop)


	/* Connect to PulseAudio */

	// 1. Request connection to the default PulseAudio server
	if (pa_context_connect(ctx->pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
		LOG(Verbosity_NORMAL, "Error: failed to connect to PulseAudio.\n");
		DEINIT();
		return AudioBackend_CONNECT_ERR;
	}

	// 2. Start the main event loop, opening communication between ctx->pa_ctx and PulseAudio
	if (pa_threaded_mainloop_start(ctx->loop) < 0) {
		DEINIT();
		return AudioBackend_LOOP_STALL;
	}


	// With our connection and event loop running, we can now get error codes/messages from PA
#undef DEINIT
#define DEINIT() \
	LOG(Verbosity_NORMAL, "PulseAudio error: %s\n", pa_strerror(pa_context_errno(ctx->pa_ctx))); \
	pa_threaded_mainloop_unlock(ctx->loop); \
	pa_threaded_mainloop_stop(ctx->loop); \
	pa_context_disconnect(ctx->pa_ctx); \
	pa_context_unref(ctx->pa_ctx); \
	pa_threaded_mainloop_free(ctx->loop)

	// 3. Wait until we get a signal that our connection state is ready to check
	pa_threaded_mainloop_wait(ctx->loop);
	if (pa_context_get_state(ctx->pa_ctx) != PA_CONTEXT_READY) {
		DEINIT();
		return AudioBackend_CONNECT_ERR;
	}

#undef DEINIT

	pa_threaded_mainloop_unlock(ctx->loop);

	LOG(Verbosity_VERBOSE, "Connected to pulseaudio.\n");

	return AudioBackend_OK;
}

static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;

	/* Disconnect from PulseAudio */

	// Stop the main loop, halting communication over our connection
	pa_threaded_mainloop_stop(ctx->loop);

	// Disconnect from the PulseAudio server and free our context
	pa_context_disconnect(ctx->pa_ctx);
	pa_context_unref(ctx->pa_ctx);

	// Free the main loop
	pa_threaded_mainloop_free(ctx->loop);

	// Disconnect the event queue
	EventQueue_free(ctx->evt_queue);
}

static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *t) {
	Ctx *ctx = ctx__;

	pa_threaded_mainloop_lock(ctx->loop);

#define DEINIT() \
	LOG(Verbosity_NORMAL, "PulseAudio error: %s\n", pa_strerror(pa_context_errno(ctx->pa_ctx))); \
	pa_threaded_mainloop_unlock(ctx->loop);


	// If a stream exists, the caller must use queue() instead
	if (ctx->stream && pa_stream_get_state(ctx->stream) != PA_STREAM_TERMINATED) {
		pa_threaded_mainloop_unlock(ctx->loop);
		return AudioBackend_STREAM_EXISTS;
	}

	// Set up our playback stream
	const AudioPCM *pcm = &t->pcm;
	pa_sample_spec sample_spec = AudioPCM_pulseaudio_spec(pcm);
	pa_channel_map channel_map = AudioPCM_pulseaudio_channel_map(pcm);
	ctx->stream = pa_stream_new(ctx->pa_ctx, BACKEND_APP_NAME, &sample_spec, &channel_map);
	if (!ctx->stream) {
		pa_threaded_mainloop_unlock(ctx->loop);;
		return AudioBackend_BAD_ALLOC;
	}

	// Set up callbacks
	pa_stream_set_state_callback(ctx->stream, pa_stream_state_cb_, ctx);
	pa_stream_set_write_callback(ctx->stream, pa_stream_write_cb_, ctx); // Write audio data for playback

#undef DEINIT
#define DEINIT() \
	LOG(Verbosity_NORMAL, "PulseAudio error: %s\n", pa_strerror(pa_context_errno(ctx->pa_ctx))); \
	pa_stream_disconnect(ctx->stream); \
	pa_stream_unref(ctx->stream); \
	pa_threaded_mainloop_unlock(ctx->loop)

	// Connect the stream

	const uint32_t buffer_ms = ctx->settings->ab_buffer_ms;
	pa_buffer_attr buf_attr = AudioPCM_pulseaudio_buffer_attr(pcm, buffer_ms);
	int status = pa_stream_connect_playback(
			ctx->stream,
			NULL,
			&buf_attr,
			PA_STREAM_START_CORKED & PA_STREAM_START_UNMUTED,
			NULL, NULL);
	if (status != 0) {
		DEINIT();

		return AudioBackend_CONNECT_ERR;
	}

	// Check stream state after connecting
	pa_threaded_mainloop_wait(ctx->loop);
	if (pa_stream_get_state(ctx->stream) != PA_STREAM_READY) {
		DEINIT();
		return AudioBackend_CONNECT_ERR;
	}

	// Connect playback buffer to framebuffer and fill framebuffer
	ctx->playback_buffer = t->buffer;
	void *tb; // Transfer buffer
	size_t tb_size = (size_t)-1;
	if (pa_stream_begin_write(ctx->stream, &tb, &tb_size) != 0) {
		DEINIT();
		return AudioBackend_FB_WRITE_ERR;
	}
	tb_size = AudioBuffer_read(ctx->playback_buffer, tb, tb_size, false);
	if (pa_stream_write(ctx->stream, tb, tb_size, NULL, 0, PA_SEEK_RELATIVE) != 0) {
		DEINIT();
		fprintf(stderr, "fuck\n");
		fprintf(stderr, "tb_size: %zu\n", tb_size);
		return AudioBackend_FB_WRITE_ERR;
	}
#undef DEINIT

	pa_threaded_mainloop_unlock(ctx->loop);

	return AudioBackend_OK;
}

static enum AudioBackend_ERR play(void *ctx__, bool pause) {
	Ctx *ctx = ctx__;

	pa_threaded_mainloop_lock(ctx->loop);

	pa_operation *op = pa_stream_cork(ctx->stream, pause, pa_stream_success_cb_, ctx);
	pa_threaded_mainloop_wait(ctx->loop);
	pa_operation_unref(op);
	const bool corked = pa_stream_is_corked(ctx->stream);

	pa_threaded_mainloop_unlock(ctx->loop);

	return (pause ^ !corked) ? AudioBackend_OK : AudioBackend_PLAY_ERR;
}

static void lock(void *ctx__) {
	Ctx *ctx = ctx__;
	pa_threaded_mainloop_lock(ctx->loop);
}

static void unlock(void *ctx__) {
	Ctx *ctx = ctx__;
	pa_threaded_mainloop_unlock(ctx->loop);
}

static void seek(void *ctx__) {
	Ctx *ctx = ctx__;

	if (!(ctx->stream && ctx->playback_buffer)) {
		return;
	}


	pa_threaded_mainloop_lock(ctx->loop);

	void *tb; // Transfer buffer
	size_t tb_size = AudioBuffer_max_read(ctx->playback_buffer, -1, -1, 0);
	if (pa_stream_begin_write(ctx->stream, &tb, &tb_size) != 0 || tb == NULL) {
		LOG(Verbosity_NORMAL, "Warning: failed to allocate PulseAudio framebuffer.\n");
		return;
	}
	tb_size = AudioBuffer_read(ctx->playback_buffer, tb, tb_size, false);
	if (pa_stream_write(ctx->stream, tb, tb_size, NULL, 0, PA_SEEK_RELATIVE_ON_READ) != 0) {
		LOG(Verbosity_NORMAL, "Error in PulseAudio seek\n");
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

	pa_threaded_mainloop_unlock(ctx->loop);
}

static void pa_ctx_state_cb_(pa_context *pa_ctx, void *userdata) {
	Ctx *ctx = userdata;

	switch (pa_context_get_state(pa_ctx)) {
	case PA_CONTEXT_READY:
	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		pa_threaded_mainloop_signal(ctx->loop, 0);
	
	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	}
}

static void pa_stream_write_cb_(pa_stream *stream, size_t n_bytes, void *userdata) {
	Ctx *ctx = userdata;

	if (!(ctx->stream && ctx->playback_buffer)) {
		return;
	}

	// Allocate destination buffer in PA server memory to minimize copying
	void *tb; // Transfer buffer
	size_t tb_size = n_bytes;
	if (pa_stream_begin_write(ctx->stream, &tb, &tb_size) != 0 || tb == NULL) {
		fprintf(stderr, "Warning: failed to populated PulseAudio framebuffer.\n");
		return;
	}
	tb_size = AudioBuffer_read(ctx->playback_buffer, tb, tb_size, false);
	if (pa_stream_write(ctx->stream, tb, tb_size, NULL, 0, PA_SEEK_RELATIVE) != 0) {
		fprintf(stderr, "Error in write callback\n");
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

static void pa_stream_state_cb_(pa_stream *stream, void *userdata) {
	Ctx *ctx = userdata;

	switch (pa_stream_get_state(stream)) {
	case PA_STREAM_READY:
	case PA_STREAM_TERMINATED:
	case PA_STREAM_FAILED:
		pa_threaded_mainloop_signal(ctx->loop, 0);
		break;
	
	case PA_STREAM_UNCONNECTED:
	case PA_STREAM_CREATING:
		break;
	}
}

static void pa_stream_success_cb_(pa_stream *stream, int success, void *userdata) {
	Ctx *ctx = userdata;

	pa_threaded_mainloop_signal(ctx->loop, 0);
}
