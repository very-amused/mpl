#include "buffer_thread.h"
#include "audio/track.h"
#include "error.h"

#ifdef __unix__
#include <semaphore.h>
#include "libdill.h"
#endif

struct BufferThread {
	AudioTrack *track;
	AudioTrack *next_track;

	int thread_handle;
	sem_t pause; // Tell the thread to temporarily stop buffering. The next notification to pause will wake the thread back up

	sem_t done; // Receive confirmation that the thread has handled a message (i.e pause)
};

BufferThread *BufferThread_new() {
	BufferThread *bt = malloc(sizeof(BufferThread));

	bt->thread_handle = -1;

	// Initialize semaphores
	sem_init(&bt->pause, 0, 0);
	sem_init(&bt->done, 0, 0);

	return bt;
}
void BufferThread_free(BufferThread *bt) {
	// Stop thread
	if (bt->thread_handle != -1) {
		dill_hclose(bt->thread_handle);
		dill_bundle_wait(bt->thread_handle, 100);
	}

	// We don't own the track pointers, so we do nothing with those

	free(bt);
}

static coroutine void BufferThread_routine(BufferThread *bt) {
	// Start buffering from bt->track
	enum AudioTrack_ERR at_err;
buffer_loop:
	do {
		int pauseval;
		sem_getvalue(&bt->pause, &pauseval);
		if (pauseval) {
			fprintf(stderr, "paused\n");
			sem_post(&bt->done);
			sem_wait(&bt->pause);
		}

		at_err = AudioTrack_buffer_packet(bt->track, NULL);
		static int packetno = 0;
		packetno++;
		fprintf(stderr, "Buffering packet %d\n", packetno);
	} while (at_err == AudioTrack_OK);

	if (!bt->next_track) {
		// Self-detach
		sem_post(&bt->done);
	}

	bt->track = bt->next_track;
	bt->next_track = NULL;
	goto buffer_loop;
}

int BufferThread_start(BufferThread *bt, AudioTrack *track, AudioTrack *next_track) {
	// Pause the thread
	if (bt->thread_handle != -1) {
		sem_post(&bt->pause);
		sem_wait(&bt->done);
	}

	// Load the track(s)
	bt->track = track;
	bt->next_track = next_track;

	// Start or resume buffering
	if (bt->thread_handle != -1) {
		sem_post(&bt->pause);
		return 0;
	}

	bt->thread_handle = dill_go(BufferThread_routine(bt));
	sem_wait(&bt->done);
	return 0;
}
