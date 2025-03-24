#include "backend.h"
#include "audio/pcm.h"

// Initialize an AudioBackend for playback
int AudioBackend_init(AudioBackend *ab, const AudioPCM *pcm) {
	// Allocate and initialize ctx
	ab->ctx = malloc(ab->ctx_size);
	if (ab->ctx == NULL) {
		fprintf(stderr, "Error: failed to allocate audio backend context.\n");
		return 1;
	}

	return ab->init(ab->ctx, pcm);
}

// Deinitialize an AudioBackend
void AudioBackend_deinit(AudioBackend *ab) {
	ab->deinit(ab->ctx);

	free(ab->ctx);
}

int Audiobackend_prepare(AudioBackend *ab, AudioTrack *track) {
	return ab->prepare(ab->ctx, track);
}

int AudioBackend_play(AudioBackend *ab, bool pause) {
	return ab->play(ab->ctx, pause);
}
