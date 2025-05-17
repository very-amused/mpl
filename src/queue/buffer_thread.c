#include "buffer_thread.h"
#include "audio/track.h"
#include "error.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

#ifdef __unix__
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#endif

struct BufferThread {
	_Atomic (AudioTrack *) track;
	_Atomic (AudioTrack *) next_track;

	pthread_t *thread;
	sem_t play; // Play/pause the thread's buffering. After posting, spin on t->paused to verify result
	atomic_bool paused; // Bool indicating whether the thread is paused (waiting on t->play)
	sem_t cancel; // Tell the thread to exit
};

BufferThread *BufferThread_new() {
	BufferThread *thr = malloc(sizeof(BufferThread));
	thr->track = thr->next_track = NULL;
	thr->thread = NULL;
	thr->paused = false;

	// Initialize semaphores
	sem_t *const semaphores[] = {&thr->play, &thr->cancel};
	static const size_t semaphores_len = sizeof(semaphores) / sizeof(semaphores[0]);
	for (size_t i = 0; i < semaphores_len; i++) {
		sem_init(semaphores[i], 0, 0);
	}

	return thr;
}
void BufferThread_free(BufferThread *thr) {
	// Stop thread
	if (thr->thread) {
		sem_post(&thr->cancel);
		BufferThread_play(thr, true);
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
		// Handle cancel signal
		if (sem_trywait(&thr->cancel) == 0) {
			//fprintf(stderr, "BufferThread has received cancel signal\n");
			return NULL;
		}
		// Handle pause signal
		if (sem_trywait(&thr->play) == 0) {
			atomic_store(&thr->paused, true);
			sem_wait(&thr->play);
			atomic_store(&thr->paused, false);
			continue;
		}

		AudioTrack *track = atomic_load(&thr->track);
		static const AudioTrack *tr_cached = NULL;
		static size_t buf_ahead_max = 0;
		if (tr_cached != track) {
			tr_cached = track;
			buf_ahead_max = track->buffer->size/2;
		}
		const int rd = atomic_load(&track->buffer->rd);
		const int wr = atomic_load(&track->buffer->wr);
		// Keep half the buffer full of past frames to enable bidirectional buffer seeks 
		// FIXME: potential performance issues
		if (AudioBuffer_max_read(track->buffer, rd, wr, false) >= buf_ahead_max) {
			sem_wait(&track->buffer->rd_sem);
			continue;
		}

		at_err = AudioTrack_buffer_packet(track, NULL);
		//static int packetno = 0;
		//packetno++;
		//fprintf(stderr, "Buffering packet %d\n", packetno);
	} while (at_err == AudioTrack_OK);

	AudioTrack *next = atomic_exchange(&thr->next_track, NULL);

	if (!next) {
		// Pause our thread until thr->track is set
		sem_post(&thr->play);
	} else {
		atomic_store(&thr->track, next);
	}
	goto buffer_loop;
}

int BufferThread_start(BufferThread *thr, AudioTrack *track, AudioTrack *next_track) {
	// Load track(s)
	atomic_store(&thr->track, track);
	atomic_store(&thr->next_track, next_track);

	// Resume buffering
	if (thr->thread) {
		BufferThread_play(thr, true);
		return 0;
	}

	// Start buffering
	thr->thread = malloc(sizeof(pthread_t));
	pthread_create(thr->thread, NULL, BufferThread_routine, thr);
	return 0;
}

void BufferThread_play(BufferThread *thr, bool play) {
	if (play) {
		// Debounce play signals
		if (!atomic_load(&thr->paused)) {
			return;
		}

		sem_post(&thr->play);
		while (atomic_load(&thr->paused)) {}
		printf("Buffering resumed\n");
	} else {
		// Debounce
		if (atomic_load(&thr->paused)) {
			return;
		}

		// Send pause signal
		sem_post(&thr->play);
		// Wake up anything blocking on a read (which could potentially include the buffer thread)
		AudioTrack *tr = atomic_load(&thr->track);
		if (tr) {
			sem_post(&tr->buffer->rd_sem);
		}
		while (!atomic_load(&thr->paused)) {}
		printf("Buffering paused\n");
	}
}
