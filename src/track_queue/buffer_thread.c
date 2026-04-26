#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "buffer_thread.h"
#include "audio/track.h"
#include "error.h"
#include "util/log.h"
#include "util/thread_rc.h"

struct BufferThread {
	pthread_t *thread;
	ThreadRC *thread_rc;
	AudioTrack *track;
	ssize_t prebuf_ms; // # of milliseconds to buffer before stopping. Used only in BufferThread_prebuffer_routine
};

static void BufferThread_wake(void *ud) {
	BufferThread *thr = ud;

	AudioTrack *tr = thr->track;
	if (tr) {
		// the BufferThread might be sleeping waiting for a buffer read
		sem_post(&tr->buffer->rd_sem);
	}
}

BufferThread *BufferThread_new() {
	BufferThread *thr = malloc(sizeof(BufferThread));
	CHECK_ALLOC(thr, NULL);
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
		AudioTrack *track = thr->track;
		if (track == NULL) {
			ThreadRC_selflock(thr->thread_rc, 0, "No track");
			continue;
		}
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
			thr->track = NULL;
			continue;
		}
	}

	pthread_exit(NULL);
}

static void *BufferThread_prebuffer_routine(void *args) {
	BufferThread *thr = args;

	while (ThreadRC_preloop(thr->thread_rc)) {
		AudioTrack *track = thr->track; // TODO I don't think atomics are doing anything here since we need to lock when track swapping regardless
		if (track == NULL) {
			ThreadRC_selflock(thr->thread_rc, 0, "No track");
			continue;
		}
		static const AudioTrack *tr_cached = NULL;
		static size_t prebuf_frames = 0;
		if (tr_cached != track) {
			tr_cached = track;
			prebuf_frames = AudioPCM_buffer_size(&track->buf_pcm, thr->prebuf_ms);
		}

		enum AudioTrack_ERR at_err = AudioTrack_buffer_packet(track, NULL);
		if (at_err != AudioTrack_OK || track->buffer->n_written >= prebuf_frames) {
			// Enter self-lock fail state
			ThreadRC_selflock(thr->thread_rc, at_err, AudioTrack_ERR_name(at_err));
			thr->track = NULL;
			continue;
		}
	}

	pthread_exit(NULL);
}

int BufferThread_start(BufferThread *thr, AudioTrack *track) {
	// Restart buffering on new track
	if (thr->thread) {
		ThreadRC_lock(thr->thread_rc);
		thr->track = track;
		ThreadRC_unlock(thr->thread_rc);
		ThreadRC_recover(thr->thread_rc);
		return 0;
	}

	// Start buffering on new track (create thread)
	thr->thread = malloc(sizeof(pthread_t));
	CHECK_ALLOC(thr->thread, 1);
	thr->track = track;
	return pthread_create(thr->thread, NULL, BufferThread_routine, thr);
}


int BufferThread_start_prebuf(BufferThread *thr, AudioTrack *track, uint32_t prebuf_ms) {
	// Restart prebuf on new track
	if (thr->thread) {
		ThreadRC_lock(thr->thread_rc);
		thr->track = track;
		ThreadRC_unlock(thr->thread_rc);
		ThreadRC_recover(thr->thread_rc);
		return 0;
	}

	// Start prebuf on new track (create thread)
	thr->prebuf_ms = prebuf_ms;
	thr->thread = malloc(sizeof(pthread_t));
	CHECK_ALLOC(thr->thread, 1);
	thr->track = track;
	return pthread_create(thr->thread, NULL, BufferThread_prebuffer_routine, thr);
}

int BufferThread_stop_prebuf(BufferThread *thr) {
	if (!thr->thread) {
		fprintf(stderr, "we've lost the plot\n");
		return 1;
	}

	return BufferThread_start_prebuf(thr, NULL, -1);
}

const AudioTrack *BufferThread_cur_track(BufferThread *thr) {
	AudioTrack *tr = NULL;
	if (thr->thread) {
		ThreadRC_lock(thr->thread_rc);
		tr = thr->track;
		ThreadRC_unlock(thr->thread_rc);
	}
	return tr;
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
