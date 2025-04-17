#include "backend.h"
#include "audio/pcm.h"
#include "ui/event_queue.h"
#include <string.h>

// Initialize an AudioBackend for playback
int AudioBackend_init(AudioBackend *ab, const EventQueue *eq) {
	// Allocate and zero ctx
	ab->ctx = malloc(ab->ctx_size);
	if (ab->ctx == NULL) {
		fprintf(stderr, "Error: failed to allocate audio backend context.\n");
		return 1;
	}
	// Zeroing ctx allows us to always recover an error state using ab->deinit,
	// including error states occuring at any point in ab->init
	memset(ab->ctx, 0, ab->ctx_size);

	return ab->init(ab->ctx, eq);
}

// Deinitialize an AudioBackend
void AudioBackend_deinit(AudioBackend *ab) {
	ab->deinit(ab->ctx);

	free(ab->ctx);
}

int AudioBackend_prepare(AudioBackend *ab, AudioTrack *track) {
	return ab->prepare(ab->ctx, track);
}

int AudioBackend_play(AudioBackend *ab, bool pause) {
	return ab->play(ab->ctx, pause);
}
