#include "loop.h"
#include "util/log.h"
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>

void DummyLoop_init(DummyLoop *loop, const AudioBuffer *ab_buf) {
	loop->ab_buf = ab_buf;
	loop->thread = NULL;
	loop->paused = false;

	// Initialize semaphores
	sem_t *const semaphores[] = {&loop->play, &loop->cancel};
	static const size_t semaphores_len = sizeof(semaphores) / sizeof(semaphores[0]);
	for (size_t i = 0; i < semaphores_len; i++) {
		sem_init(semaphores[i], 0, 0);
	}

	// Initialize locks and CVs
	pthread_mutex_init(&loop->paused_lock, NULL);
	pthread_cond_init(&loop->paused_cv, NULL);
}
void DummyLoop_deinit(DummyLoop *loop) {
	// Stop thread
	if (loop->thread) {
		// Load a cancel message for the thread to read
		sem_post(&loop->cancel);
		// Ensure the thread is awake to read and react to it
		DummyLoop_play(loop, true);
		pthread_join(*loop->thread, NULL);
	}
	free(loop->thread);

	// We don't own the backend's internal buffer, so we do nothing with it
}

void DummyLoop_play(DummyLoop *loop, bool play) {
	pthread_mutex_lock(&loop->paused_lock);

	if (play) {
		// Debounce
		if (!loop->paused) {
			pthread_mutex_unlock(&loop->paused_lock);
			return;
		}

		// Send play signal
		sem_post(&loop->play);
		pthread_cond_wait(&loop->paused_cv, &loop->paused_lock);
	} else {
		// Debounce
		if (loop->paused) {
			pthread_mutex_unlock(&loop->paused_lock);
			return;
		}

		// Send pause signal
		sem_post(&loop->play);
		pthread_cond_wait(&loop->paused_cv, &loop->paused_lock);
		LOG(Verbosity_DEBUG, "DummyLoop paused (debug audio backend)\n");
	}

	pthread_mutex_unlock(&loop->paused_lock);
}

static void *DummyLoop_routine(void *args) {
	DummyLoop *loop = args;

	// TODO: we need a sibling thread that executes callbacks on demand, and sleeps until given a callback
	// All routines that call callbacks, henceforth called 'event handlers', must accept a single argument of type (DummyLoop *).
	// Additionally, we need a signalling system for both 'we need to run a callback' and 'a callback has finished running'
	// This is some moderate complexity in service of compacting a loop/stream/server architecture into just a loop

	// Start reading from loop->ab_buf,
	// 
	return NULL;
}

int DummyLoop_start(DummyLoop *loop) {
	if (loop->thread) {
		// DummyLoop_play must be used after the loop is started
		return 1;
	}

	// Create and start the loop
	loop->thread = malloc(sizeof(pthread_t));
	pthread_create(loop->thread, NULL, DummyLoop_routine, loop);
	return 0;
}

