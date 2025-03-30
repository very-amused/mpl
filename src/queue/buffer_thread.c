#include "buffer_thread.h"
#include "audio/track.h"

#ifdef __unix__
#include <pthread.h>
#include <semaphore.h>
#endif

struct BufferThread {
	AudioTrack *playback_track;
	AudioTrack *next_track;

	pthread_t *thread;
	sem_t pause; // Tell the thread to temporarily stop buffering. The next notification to pause will wake the thread back up
	sem_t cancel; // Tell the thread to exit
};

BufferThread *BufferThread_new() {
	BufferThread *bt = malloc(sizeof(BufferThread));
	bt->playback_track = bt->next_track = NULL;
	bt->thread = NULL;

	// Initialize semaphores
	sem_init(&bt->pause, 0, 1);
	sem_init(&bt->cancel, 0, 1);

	return bt;
}
void BufferThread_free(BufferThread *bt) {
	// Stop thread
	if (bt->thread) {
		sem_post(&bt->cancel);
		pthread_join(*bt->thread, NULL);
		bt->thread = NULL;
	}

	// We don't own the track pointers, so we do nothing with those

	free(bt);
}
