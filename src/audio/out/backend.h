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
	void (*deinit)(void *ctx);

	// Private backend-specific context
	const size_t ctx_size;
	void *ctx;
} AudioBackend;


// Initialize an AudioBackend for playback
int AudioBackend_init(AudioBackend *ab);

// Deinitialize an AudioBackend
void AudioBackend_deinit(AudioBackend *ab);
