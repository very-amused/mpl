#include "backend.h"
#include <pulse/def.h>
#include <pulse/proplist.h>
#include <pulse/thread-mainloop.h>
#include <pulse/context.h>
#include <pulse/stream.h>
#include <pulse/error.h>

typedef struct Ctx {
	// Asynchronous event loop
	pa_threaded_mainloop *loop;

	// PA connection properties
	pa_proplist *props;
	// PA connection context
	pa_context *conn;
	pa_context_state_t conn_state;

	// Audio playback stream
	pa_stream *stream;
} Ctx;

/* PulseAudio AudioBackend methods */
static int init(void *ctx__);
static void deinit(void *ctx__);


/* AudioBackend implementation using PulseAudio */
AudioBackend AB_PulseAudio = {
	.name = "PulseAudio",

	.init = init,
	.deinit = deinit,

	.ctx_size = sizeof(Ctx)
};

/* PulseAudio callbacks */
// Connection state change callback
static void context_state_callback(pa_context *conn_ctx, void *userdata);

static int init(void *ctx__) {
	Ctx *ctx = ctx__;

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
	ctx->props = pa_proplist_new();
	if (!ctx->props) {
		fprintf(stderr, "Error: failed to create PulseAudio property list.\n");
		DEINIT();
		return 1;
	}
	ctx->conn = pa_context_new_with_proplist(
			pa_threaded_mainloop_get_api(ctx->loop),
			BACKEND_APP_NAME,
			ctx->props);
	if (!ctx->conn) {
		fprintf(stderr, "Error: failed to create PulseAudio connection context.\n");
		DEINIT();
		return 1;
	}
	
	// Set context state change callback,
	// which will send us back a signal when we've conclusively succeeded or failed at connecting to the server
	pa_context_set_state_callback(ctx->conn, context_state_callback, ctx);

#undef DEINIT
#define DEINIT() \
	pa_context_disconnect(ctx->conn); \
	pa_context_unref(ctx->conn); \
	pa_threaded_mainloop_unlock(ctx->loop); \
	pa_threaded_mainloop_free(ctx->loop)


	/* Connect to PulseAudio */

	// 1. Request connection to the default PulseAudio server
	if (pa_context_connect(ctx->conn, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
		fprintf(stderr, "Error: failed to connect to PulseAudio.\n");
		DEINIT();
		return 1;
	}

	// 2. Start the main event loop, opening communication between ctx->conn and PulseAudio
	if (pa_threaded_mainloop_start(ctx->loop) < 0) {
		fprintf(stderr, "Error: failed to start PulseAudio main loop.\n");
		DEINIT();
		return 1;
	}


	// With our connection and event loop running, we can now get error codes/messages from PA
#undef DEINIT
#define DEINIT() \
	fprintf(stderr, "PulseAudio error: %s\n", pa_strerror(pa_context_errno(ctx->conn))); \
	pa_threaded_mainloop_unlock(ctx->loop); \
	pa_threaded_mainloop_stop(ctx->loop); \
	pa_context_disconnect(ctx->conn); \
	pa_context_unref(ctx->conn); \
	pa_threaded_mainloop_free(ctx->loop)

	// 3. Wait until we get a signal that our connection state is ready to check
	pa_threaded_mainloop_wait(ctx->loop);
	if (pa_context_get_state(ctx->conn) != PA_CONTEXT_READY) {
		fprintf(stderr, "Error: failed to connect to PulseAudio.\n");
		DEINIT();
		return 1;
	}

	pa_threaded_mainloop_unlock(ctx->loop);
	fprintf(stderr, "Connected to PulseAudio!\n");

	// TODO: set up our playback stream

#undef DEINIT

	return 0;
}

static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;

	/* Disconnect from PulseAudio */

	// Stop the main loop, halting communication over our connection
	pa_threaded_mainloop_stop(ctx->loop);

	// Disconnect from the PulseAudio server and free our context
	pa_context_disconnect(ctx->conn);
	pa_context_unref(ctx->conn);
	pa_proplist_free(ctx->props);

	// Free the main loop
	pa_threaded_mainloop_free(ctx->loop);
}

static void context_state_callback(pa_context *conn, void *userdata) {
	Ctx *ctx = userdata;

	switch (pa_context_get_state(conn)) {
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
