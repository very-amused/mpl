#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define BACKEND_APP_NAME "mpl"

// Functions indicating success by returning an int
// return 0 on success, nonzero on error

// Audio output interface implemented by all audio backends.
// Q: *how* are we *playing* our audio
typedef struct AudioBackend {
	const char *name; // Backend name

	// Initialize the audio backend for playback.
	int (*init)(void *ctx);
	// Deinitialize the audio backend
	int (*deinit)(void *ctx);

	// Private backend-specific context
	const size_t ctx_size;
	void *ctx;
} AudioBackend;


// Initialize an AudioBackend for playback
static int AudioBackend_init(AudioBackend *ab) {
	// Allocate and initialize ctx
	ab->ctx = malloc(ab->ctx_size);
	if (ab->ctx == NULL) {
		fprintf(stderr, "Error: failed to allocate audio backend context.\n");
		return 1;
	}

	return ab->init(ab->ctx);
}

// Deinitialize an AudioBackend
static int AudioBackend_deinit(AudioBackend *ab) {
	int status = ab->deinit(ab->ctx);
	if (status != 0) {
		return status;
	}

	free(ab->ctx);

	return 0;
}
