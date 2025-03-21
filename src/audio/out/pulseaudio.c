#include <pulse/channelmap.h>
#include <pulse/def.h>
#include <pulse/proplist.h>
#include <pulse/sample.h>
#include <pulse/thread-mainloop.h>
#include <pulse/context.h>
#include <pulse/stream.h>
#include <pulse/error.h>
#include <stddef.h>

#include "audio/buffer.h"
#include "audio/pcm.h"
#include "backend.h"

// PulseAudio backend context
typedef struct Ctx {
	// Asynchronous event loop
	pa_threaded_mainloop *loop;

	// PA connection context
	pa_context *pa_ctx;

	// Audio playback stream
	pa_stream *stream;
	// PA can't accept planar samples afaict, so we need to know if we need to interlace them.
	AudioPCM PCM;

	// Current audio playback buffer
	AudioBuffer *const *playback_buffer;
} Ctx;

/* PulseAudio AudioBackend methods */
static int init(void *ctx__, const AudioPCM *pcm);
static void deinit(void *ctx__);

/* AudioBackend implementation using PulseAudio */
AudioBackend AB_PulseAudio = {
	.name = "PulseAudio",

	.init = init,
	.deinit = deinit,

	.ctx_size = sizeof(Ctx)
};

/* PulseAudio callbacks. *userdata is of type *Ctx. */
// Connection state change callback
static void pa_ctx_state_cb_(pa_context *pa_ctx, void *userdata);
// Audio data write callback
static void pa_stream_write_cb_(pa_stream *stream, size_t n_bytes, void *userdata);
// Audio stream state change callback
static void pa_stream_state_cb_(pa_stream *stream, void *userdata);

static int init(void *userdata, const AudioPCM *pcm) {
	Ctx *ctx = userdata;

	/* Set up our PulseAudio event loop and connection objects */

	// Allocate the main event loop, obtain lock for loop initialization
	ctx->loop = pa_threaded_mainloop_new();
	if (!ctx->loop) {
		fprintf(stderr, "Error: failed to create PulseAudio main loop.\n");
		return 1;
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
		fprintf(stderr, "Error: failed to create PulseAudio connection context.\n");
		DEINIT();
		return 1;
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
		fprintf(stderr, "Error: failed to connect to PulseAudio.\n");
		DEINIT();
		return 1;
	}

	// 2. Start the main event loop, opening communication between ctx->pa_ctx and PulseAudio
	if (pa_threaded_mainloop_start(ctx->loop) < 0) {
		fprintf(stderr, "Error: failed to start PulseAudio main loop.\n");
		DEINIT();
		return 1;
	}


	// With our connection and event loop running, we can now get error codes/messages from PA
#undef DEINIT
#define DEINIT() \
	fprintf(stderr, "PulseAudio error: %s\n", pa_strerror(pa_context_errno(ctx->pa_ctx))); \
	pa_threaded_mainloop_unlock(ctx->loop); \
	pa_threaded_mainloop_stop(ctx->loop); \
	pa_context_disconnect(ctx->pa_ctx); \
	pa_context_unref(ctx->pa_ctx); \
	pa_threaded_mainloop_free(ctx->loop)

	// 3. Wait until we get a signal that our connection state is ready to check
	pa_threaded_mainloop_wait(ctx->loop);
	if (pa_context_get_state(ctx->pa_ctx) != PA_CONTEXT_READY) {
		fprintf(stderr, "Error: failed to connect to PulseAudio.\n");
		DEINIT();
		return 1;
	}

	fprintf(stderr, "Connected to PulseAudio!\n");

	// Set up our playback stream
	pa_sample_spec sample_spec = AudioPCM_pulseaudio_spec(pcm);
	pa_channel_map channel_map = AudioPCM_pulseaudio_channel_map(pcm);
	ctx->stream = pa_stream_new(ctx->pa_ctx, BACKEND_APP_NAME, &sample_spec, &channel_map);
	if (!ctx->stream) {
		fprintf(stderr, "Error: failed to create PulseAudio stream.\n");
		DEINIT();
		return 1;
	}

	// Set up callbacks
	pa_stream_set_write_callback(ctx->stream, pa_stream_write_cb_, ctx); // Write audio data for playback
	pa_stream_set_state_callback(ctx->stream, pa_stream_state_cb_, ctx);

	// TODO: Connect stream


#undef DEINIT

	pa_threaded_mainloop_unlock(ctx->loop);

	return 0;
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

	// Pull data from playback buffer
	AudioBuffer *cur_buffer = *ctx->playback_buffer;
	if (!cur_buffer) {
		pa_stream_cork(stream, 1, NULL, NULL);
	}

	// Allocate destination buffer in PA server memory to minimize copying
	unsigned char *dst;
	pa_stream_begin_write(stream, (void **)&dst, &n_bytes);
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
