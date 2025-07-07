#include <pthread.h>
#include <stdlib.h>

#include "loop.h"
#include "audio/pcm.h"

struct DummyLoop {
	// Thread that runs the loop's clock asynchronously from the main thread
	pthread_t thread;
	// Lock over the loop's data. The loop holds this itself most of the time,
	// and the main thread only locks it to call DummyLoop_* methods.
	pthread_mutex_t lock;
	// CV that wakes up a stopped loop (main thread -> loop)
	pthread_cond_t wake_cv;
	// CV that signals state changes by unblocking DummyLoop_wait() (loop thread -> main)
	pthread_cond_t signal_cv;

	/* Callbacks (run on async thread with lock pre-acquired) */
	// Audio data write callback (set w/ DummyLoop_set_write_callback)
	void (*write_cb)(DummyLoop *loop, size_t n_bytes, void *userdata);
};

