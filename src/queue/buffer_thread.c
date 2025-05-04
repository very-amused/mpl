#include "buffer_thread.h"
#include "audio/track.h"
#include "error.h"
#include <errno.h>
#include <stdatomic.h>
#include <stddef.h>

#ifdef __unix__
#include <pthread.h>
#include <semaphore.h>
#endif

struct BufferThread {
	_Atomic (AudioTrack *) track;
	_Atomic (AudioTrack *) next_track;

	pthread_t *thread;
	sem_t pause; // Tell the thread to temporarily stop buffering.
	sem_t play; // Tell a paused thread to resume buffering
	sem_t cancel; // Tell the thread to exit
};

BufferThread *BufferThread_new() {
	BufferThread *thr = malloc(sizeof(BufferThread));
	thr->track = thr->next_track = NULL;
	thr->thread = NULL;

	// Initialize semaphores
	sem_t *const semaphores[] = {&thr->pause, &thr->play, &thr->cancel};
	const size_t semaphores_len = sizeof(semaphores) / sizeof(semaphores[0]);
	for (size_t i = 0; i < semaphores_len; i++) {
		sem_init(semaphores[i], 0, 0);
	}

	return thr;
}
void BufferThread_free(BufferThread *thr) {
	// Stop thread
	if (thr->thread) {
		sem_post(&thr->cancel);
		int paused;
		sem_getvalue(&thr->pause, &paused);
		if (paused) {
			atomic_store(&thr->track, NULL);
			sem_post(&thr->play);
		}
		pthread_join(*thr->thread, NULL);
	}
	free(thr->thread);

	// We don't own the track pointers, so we do nothing with those

	free(thr);
}

static void *BufferThread_routine(void *args) {
	BufferThread *thr = args;

	// Start buffering from thr->track
	enum AudioTrack_ERR at_err;
buffer_loop:
	do {
		if (sem_trywait(&thr->cancel) == 0) {
			return NULL;
		}
		int paused;
		sem_getvalue(&thr->pause, &paused);
		if (paused) {
			// Keep pause at 1 until unpaused (so other threads can see we're paused)
			sem_wait(&thr->play);
			sem_wait(&thr->pause);
			goto buffer_loop;
		}

		AudioTrack *track = atomic_load(&thr->track);
		at_err = AudioTrack_buffer_packet(track, NULL);
		static int packetno = 0;
		packetno++;
		//fprintf(stderr, "Buffering packet %d\n", packetno);
	} while (at_err == AudioTrack_OK);

	AudioTrack *next = atomic_exchange(&thr->next_track, NULL);

	if (!next) {
		sem_post(&thr->pause);
	} else {
		atomic_store(&thr->track, next);
	}
	goto buffer_loop;
}

int BufferThread_start(BufferThread *thr, AudioTrack *track, AudioTrack *next_track) {
	// Load track(s)
	atomic_store(&thr->track, track);
	atomic_store(&thr->next_track, next_track);

	// Start or resume buffering
	if (thr->thread) {
		int paused;
		sem_getvalue(&thr->pause, &paused);
		if (paused) {
			sem_post(&thr->play);
		}
		return 0;
	}

	thr->thread = malloc(sizeof(pthread_t));
	pthread_create(thr->thread, NULL, BufferThread_routine, thr);
	return 0;
}

void BufferThread_play(BufferThread *thr, bool pause) {
	if (pause) {
		sem_post(&thr->pause);
		return;
	} else {
		int paused;
		sem_getvalue(&thr->pause, &paused);
		if (paused) {
			sem_post(&thr->play);
		}
	}
}
