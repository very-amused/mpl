#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>



#include "buffer_thread.h"
#include "audio/track.h"
#include "error.h"
#include "util/log.h"
#include "util/thread_rc.h"

struct BufferThread {
	pthread_t *thread;
	ThreadRC *thread_rc;
	_Atomic (AudioTrack *) track;
};

static void BufferThread_wake(void *ud) {
	BufferThread *thr = ud;

	AudioTrack *tr = atomic_load(&thr->track);
	if (tr) {
		// the BufferThread might be sleeping waiting for a buffer read
		sem_post(&tr->buffer->rd_sem);
	}
}

BufferThread *BufferThread_new() {
	BufferThread *thr = malloc(sizeof(BufferThread));
	memset(thr, 0, sizeof(BufferThread));
	
	ThreadRC_AntiDeadlock anti_deadlock = {
		.wake_aux_thread = BufferThread_wake
	};
	thr->thread_rc = ThreadRC_new(anti_deadlock, thr);

	return thr;
}
void BufferThread_free(BufferThread *thr) {
	// Stop thread
	if (thr->thread) {
		ThreadRC_shutdown(thr->thread_rc);
		pthread_join(*thr->thread, NULL);
	}
	ThreadRC_free(thr->thread_rc);
	free(thr->thread);

	// We don't own the track pointers, so we do nothing with those

	free(thr);
}

static void *BufferThread_routine(void *args) {
	BufferThread *thr = args;

	// Start buffering from thr->track
	while (ThreadRC_preloop(thr->thread_rc)) {
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
			// We sleep here, so it's crucial to post this semaphore
			// in the anti-deadlock for our ThreadRC.
			sem_wait(&track->buffer->rd_sem);
			continue;
		}

		enum AudioTrack_ERR at_err = AudioTrack_buffer_packet(track, NULL);
		if (at_err != AudioTrack_OK) {
			// Enter self-lock fail state
			ThreadRC_selflock(thr->thread_rc, at_err, AudioTrack_ERR_name(at_err));
			continue;
		}
	};

	pthread_exit(NULL);
}

int BufferThread_start(BufferThread *thr, AudioTrack *track) {
	// Load track
	atomic_store(&thr->track, track);

	// Restart buffering on new track
	if (thr->thread) {
		ThreadRC_recover(thr->thread_rc);
		return 0;
	}

	// Start buffering
	thr->thread = malloc(sizeof(pthread_t));
	return pthread_create(thr->thread, NULL, BufferThread_routine, thr);
}

void BufferThread_lock(BufferThread *thr) {
	ThreadRC_lock(thr->thread_rc);
	LOG(Verbosity_DEBUG, "Buffering locked\n");
}

int BufferThread_unlock(BufferThread *thr) {
	int status = ThreadRC_unlock(thr->thread_rc);
	if (status == 0) {
		LOG(Verbosity_DEBUG, "Buffering unlocked\n");
	} else if (status == EAGAIN) {
		LOG(Verbosity_DEBUG, "Buffering unlocked but not unpaused\n");
	}

	return status;
}
