#pragma once
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#include "audio/buffer.h"

// A dummy audio loop thread that uses an internal ring buffer to read audio frames at a fixed rate,
// emulating an AudioBackend async-loop setup
typedef struct DummyLoopThread {
	pthread_t *thread;

	// Event loop: thread wakes up on CV and then checks flags to see what to do
	pthread_mutex_t lock;
	pthread_cond_t cv;
	bool exit; // The loop will stop and exit
	bool run_callback; // The loop will run a user-defined callback fn
} DummyLoopThread;

// Initialize a DummyLoop for use; does not start.
// ab_buf is the audio backend's internal buffer which the loop will read from (once started)
void DummyLoop_init(DummyLoopThread *loop, const AudioBuffer *ab_buf);
// Stop a DummyLoop and free its resources
void DummyLoop_deinit(DummyLoopThread *loop);

// Play/pause a DummyLoop, blocking until the play/pause state is set as requested.
// NOTE: initially start the loop with [DummyLoop_start]
void DummyLoop_play(DummyLoopThread *loop, bool play);

// Initially start the loop, which begins reading frames from ab_buf
// Returns 0 on success, nonzero on error
//
// Once the loop is started, use DummyLoop_play() to pause/unpause it
int DummyLoop_start(DummyLoopThread *loop);

