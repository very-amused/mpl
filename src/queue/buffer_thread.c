#include "buffer_thread.h"
#include "audio/track.h"
#include "error.h"
#include <errno.h>

#ifdef __unix__
#include <pthread.h>
#include <semaphore.h>
#endif

struct BufferThread {
	AudioTrack *track;
	AudioTrack *next_track;

	pthread_t *thread;
	sem_t pause; // Tell the thread to temporarily stop buffering. The next notification to pause will wake the thread back up
	sem_t cancel; // Tell the thread to exit

	sem_t done; // Receive confirmation that the thread has handled a message (i.e pause)
};

BufferThread *BufferThread_new() {
	BufferThread *bt = malloc(sizeof(BufferThread));
	bt->track = bt->next_track = NULL;
	bt->thread = NULL;

	// Initialize semaphores
	sem_init(&bt->pause, 0, 0);
	sem_init(&bt->cancel, 0, 0);
	sem_init(&bt->done, 0, 0);

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

static void *BufferThread_routine(void *args) {
	BufferThread *bt = args;

	// Start buffering from bt->track
	enum AudioTrack_ERR at_err;
buffer_loop:
	do {
		int cancelval, pauseval;
		sem_getvalue(&bt->pause, &pauseval);
		sem_getvalue(&bt->cancel, &cancelval);
		if (cancelval) {
			fprintf(stderr, "canceled\n");
			pthread_exit(NULL);
		} else if (pauseval) {
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
		pthread_detach(*bt->thread);
		free(bt->thread);
		sem_post(&bt->done);
		pthread_exit(NULL);
	}

	bt->track = bt->next_track;
	bt->next_track = NULL;
	goto buffer_loop;
}

int BufferThread_start(BufferThread *bt, AudioTrack *track, AudioTrack *next_track) {
	// Pause the thread
	if (bt->thread) {
		sem_post(&bt->pause);
		sem_wait(&bt->done);
	}

	// Load the track(s)
	bt->track = track;
	bt->next_track = next_track;

	// Start or resume buffering
	if (bt->thread) {
		sem_post(&bt->pause);
		return 0;
	}
	bt->thread = malloc(sizeof(pthread_t));

	pthread_create(bt->thread, NULL, BufferThread_routine, bt);
	sem_wait(&bt->done);
	return 0;
}
