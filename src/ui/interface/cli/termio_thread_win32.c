#include <pthread.h>
#include <Windows.h>

#include "config/keybind/keybind_map.h"
#include "ui/event_queue.h"
#include "termio.h"
#include "termio_thread.h"
#include "termio_events.h"
#include "util/thread_rc.h"

struct TermIOThread {
	pthread_t thread;
	ThreadRC *rc;

	TermIO *io;

	// Event pipe with associated semaphore
	// (WaitForMultipleObjects can poll a semaphore but not a pipe)
	HANDLE evt_pipe[2]; // (evt_pipe[1] is the write end)
	HANDLE evt_pipe_sem;
};
