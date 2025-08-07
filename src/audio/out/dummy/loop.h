#include "audio/buffer.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

// A dummy audio loop thread that uses an internal ring buffer to read audio frames at a fixed rate,
// emulating an AudioBackend async-loop setup
typedef struct DummyLoop {
	pthread_t *thread;

	/* Playing/pausing loop: */
	sem_t play; // Play/pause the loop thread. After posting, spin on t->paused to verify result
	sem_t cancel; // Tell the thread to exit
	bool paused; // Bool indicating whether the thread is paused (waiting on t->play)
	pthread_mutex_t paused_lock; // Lock for t->paused
	pthread_cond_t paused_cv; // CV broadcasting t->pause updates

	// Dummy AudioBackend buffer that we read from
	// TODO: how many frames do we read each loop iter?
	const AudioBuffer *ab_buf;
} DummyLoop;

// Initialize a DummyLoop for use; does not start.
// ab_buf is the audio backend's internal buffer which the loop will read from (once started)
void DummyLoop_init(DummyLoop *loop, const AudioBuffer *ab_buf);
// Stop a DummyLoop and free its resources
void DummyLoop_deinit(DummyLoop *loop);

// Play/pause a DummyLoop, blocking until the play/pause state is set as requested.
// NOTE: initially start the loop with [DummyLoop_start]
void DummyLoop_play(DummyLoop *loop, bool play);

// Initially start the loop, which begins reading frames from ab_buf
// Returns 0 on success, nonzero on error
//
// Once the loop is started, use DummyLoop_play() to pause/unpause it
int DummyLoop_start(DummyLoop *loop);

