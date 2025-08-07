#include "loop.h"
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
	// TODO
}

int DummyLoop_start(DummyLoop *loop) {
	// TODO
	return 0;
}
