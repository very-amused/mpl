#pragma once
#include <stddef.h>

// Audio output interface implemented by all audio backends.
// Q: *how* are we *playing* our audio
typedef struct MPL_AudioBackend {
	const char *name; // Backend name

	// Private driver-specific context
	const size_t ctx_size;
	void *ctx;
} MPL_AudioBackend;
