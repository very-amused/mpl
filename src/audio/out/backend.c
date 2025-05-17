#include "ui/event_queue.h"
#include <string.h>

#include "backend.h"
#include "error.h"
#include "util/log.h"

// Initialize an AudioBackend for playback
enum AudioBackend_ERR AudioBackend_init(AudioBackend *ab, const EventQueue *eq) {
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
	LOG(Verbosity_VERBOSE, "Deinitializing audio backend\n");
	ab->deinit(ab->ctx);

	free(ab->ctx);
}

enum AudioBackend_ERR AudioBackend_prepare(AudioBackend *ab, AudioTrack *track) {
	return ab->prepare(ab->ctx, track);
}

enum AudioBackend_ERR AudioBackend_play(AudioBackend *ab, bool pause) {
	return ab->play(ab->ctx, pause);
}

void AudioBackend_lock(AudioBackend *ab) {
 ab->lock(ab->ctx);
 LOG(Verbosity_VERBOSE, "Audio backend locked\n");
}
void AudioBackend_unlock(AudioBackend *ab) {
	ab->unlock(ab->ctx);
	LOG(Verbosity_VERBOSE, "Audio backend unlocked\n");
}
void AudioBackend_seek(AudioBackend *ab) {
	ab->seek(ab->ctx);
}
