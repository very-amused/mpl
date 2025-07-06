#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

#include <pthread.h>
#include <semaphore.h>


#include "buffer_thread.h"
#include "audio/track.h"
#include "error.h"
#include "util/log.h"

struct BufferThread {
	_Atomic (AudioTrack *) track;
	_Atomic (AudioTrack *) next_track;

	pthread_t *thread;

	/* Playing/pausing buffering: */
	sem_t play; // Play/pause the thread's buffering. After posting, spin on t->paused to verify result
	sem_t cancel; // Tell the thread to exit
	bool paused; // Bool indicating whether the thread is paused (waiting on t->play)
	pthread_mutex_t paused_lock; // Lock for t->paused
	pthread_cond_t paused_cv; // CV broadcasting t->pause updates

	/* Recursive locking: */
	sem_t lock_sem; // Semaphore enabling recursive locking
};

BufferThread *BufferThread_new() {
	BufferThread *thr = malloc(sizeof(BufferThread));
	thr->track = thr->next_track = NULL;
	thr->thread = NULL;
	thr->paused = false;

	// Initialize semaphores
	sem_t *const semaphores[] = {&thr->play, &thr->cancel, &thr->lock_sem};
	static const size_t semaphores_len = sizeof(semaphores) / sizeof(semaphores[0]);
	for (size_t i = 0; i < semaphores_len; i++) {
		sem_init(semaphores[i], 0, 0);
	}

	// Initialize locks and CVs
	pthread_mutex_init(&thr->paused_lock, NULL);
	pthread_cond_init(&thr->paused_cv, NULL);

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
			LOG(Verbosity_VERBOSE, "Buffer thread received cancel signal\n");
			return NULL;
		}
		// Handle pause signal
		if (sem_trywait(&thr->play) == 0) {
			pthread_mutex_lock(&thr->paused_lock);
			{
				thr->paused = true;
				pthread_cond_broadcast(&thr->paused_cv);
				// Drain our semaphore before we block on it
				while (sem_trywait(&thr->play) == 0) {}
			}
			pthread_mutex_unlock(&thr->paused_lock);
			sem_wait(&thr->play);
			pthread_mutex_lock(&thr->paused_lock);
			{
				thr->paused = false;
				pthread_cond_broadcast(&thr->paused_cv);
			}
			pthread_mutex_unlock(&thr->paused_lock);
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
		if (AudioBuffer_max_read(track->buffer, rd, wr, false) >= buf_ahead_max) {
			sem_wait(&track->buffer->rd_sem);
			continue;
		}

		at_err = AudioTrack_buffer_packet(track, NULL);
	} while (at_err == AudioTrack_OK);

	AudioTrack *next = atomic_exchange(&thr->next_track, NULL);

	if (!next) {
		// Pause our thread until thr->track is set
		// NOTE: since we can't hold paused_lock here,
		// this is the only way to self-pause without risking a deadlock
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
		return BufferThread_unlock(thr);
	}

	// Start buffering
	thr->thread = malloc(sizeof(pthread_t));
	pthread_create(thr->thread, NULL, BufferThread_routine, thr);
	return 0;
}

void BufferThread_play(BufferThread *thr, bool play) {
	// Acquire lock over the 'paused' member so we can read it and listen for updates
	pthread_mutex_lock(&thr->paused_lock);

	if (play) {
		// Debounce play signals
		if (!thr->paused) {
			pthread_mutex_unlock(&thr->paused_lock);
			return;
		}

		sem_post(&thr->play);
		pthread_cond_wait(&thr->paused_cv, &thr->paused_lock);
		LOG(Verbosity_DEBUG, "Buffering resumed\n");
	} else {
		// Debounce
		if (thr->paused) {
			pthread_mutex_unlock(&thr->paused_lock);
			return;
		}

		// Send pause signal
		sem_post(&thr->play);
		// Wake up anything blocking on a read (which could potentially include the buffer thread)
		AudioTrack *tr = atomic_load(&thr->track);
		if (tr) {
			sem_post(&tr->buffer->rd_sem);
		}
		pthread_cond_wait(&thr->paused_cv, &thr->paused_lock);
		LOG(Verbosity_DEBUG, "Buffering paused\n");
	}

	pthread_mutex_unlock(&thr->paused_lock);
}

void BufferThread_lock(BufferThread *thr) {
	// Increment # of lockers
	sem_post(&thr->lock_sem);

	// Lock our pause state
	pthread_mutex_lock(&thr->paused_lock);

	// If the thread is already paused, we don't need to do anything
	if (thr->paused) {
		pthread_mutex_unlock(&thr->paused_lock);
		return;
	}

	// Send pause signal
	sem_post(&thr->play);
	while (!thr->paused) {
		// Wake up anything blocking on a read (which could potentially include the buffer thread)
		AudioTrack *tr = atomic_load(&thr->track);
		if (tr) {
			sem_post(&tr->buffer->rd_sem);
		}
		pthread_cond_wait(&thr->paused_cv, &thr->paused_lock);
	}
	LOG(Verbosity_DEBUG, "Buffering locked\n");

	pthread_mutex_unlock(&thr->paused_lock);
}

// Unlock a BufferThread.
// Recursive locking is enabled through an internal semaphore.
// NOTE: BufferThread_unlock() will return EAGAIN when the thread hasn't actually been unlocked.
// NOTE: On success, BufferThread_unlock() will restore the pause state of the BufferThread from when it was *originally locked*
//
// Returns 0 on success.
int BufferThread_unlock(BufferThread *thr) {
	pthread_mutex_lock(&thr->paused_lock);

	// Decrement locker count
	if (sem_trywait(&thr->lock_sem) != 0) {
		if (errno == EAGAIN) {
			LOG(Verbosity_VERBOSE, "Error: extraneous BufferThread_unlock() call\n");
		}
		pthread_mutex_unlock(&thr->paused_lock);
		return 1;
	}

	// Don't resume buffering if there are still lockers
	int n_lockers;
	sem_getvalue(&thr->lock_sem, &n_lockers);
	if (n_lockers > 0) {
		pthread_mutex_unlock(&thr->paused_lock);
		return EAGAIN;
	}

	if (!thr->paused) {
		LOG(Verbosity_VERBOSE, "Error: BufferThread woke up prematurely before final unlock\n");
		pthread_mutex_unlock(&thr->paused_lock);
		return 1;
	}
	// Send play signal
	sem_post(&thr->play);
	while (thr->paused) {
		pthread_cond_wait(&thr->paused_cv, &thr->paused_lock);
	}
	LOG(Verbosity_DEBUG, "Buffering unlocked\n");

	pthread_mutex_unlock(&thr->paused_lock);

	return 0;
}
