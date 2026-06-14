#include <pthread.h>
#include <handleapi.h>
#include <namedpipeapi.h>
#include <consoleapi.h>
#include <processenv.h>
#include <stdlib.h>
#include <Windows.h>
#define stdin_handle GetStdHandle(STD_INPUT_HANDLE)
#define stderr_handle GetStdHandle(STD_ERROR_HANDLE)

#include "config/keybind/keybind_map.h"
#include "error.h"
#include "ui/event_queue.h"
#include "termio.h"
#include "termio_thread.h"
#include "termio_events.h"
#include "util/log.h"
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

static void *TermIOThread_loop(void *thr__);

static void send_event(TermIOThread *thr, const TermIO_Event *evt) {
	WriteFile(thr->evt_pipe[1], evt, sizeof(TermIO_Event), NULL, NULL);
	// This posts (increases) the semaphore,
	// ReleaseSemaphore is a poor choice of name
	ReleaseSemaphore(thr->evt_pipe_sem, 1, NULL);
}

static void TermIOThread_wake(void *ud) {
	TermIOThread *thr = ud;
	if (!(thr->evt_pipe[1] && thr->evt_pipe_sem)) {
		return;
	}
	static const TermIO_Event wake_evt = {
		.event_type = TermIO_THREADRC_WAKE
	};
	send_event(thr, &wake_evt);
}

TermIOThread *TermIOThread_new(EventQueue *eq, KeybindMap *keybinds) {
	TermIOThread *thr = malloc(sizeof(TermIOThread));
	CHECK_ALLOC(thr, NULL);
	memset(thr, 0, sizeof(TermIOThread));

	ThreadRC_AntiDeadlock anti_deadlock = {
		.wake_aux_thread = TermIOThread_wake
	};
	thr->rc = ThreadRC_new(anti_deadlock, thr);
	if (!thr->rc) {
		free(thr);
		return NULL;
	}
	thr->io = TermIO_new(eq, keybinds);
	if (!thr->io) {
		free(thr->rc);
		free(thr);
		return NULL;
	}

	thr->evt_pipe_sem = CreateSemaphoreA(NULL, 0, -1u >> 1, NULL);
	if (!thr->evt_pipe_sem) {
		LOG(Verbosity_VERBOSE, "Failed to create TermIO event pipe semaphore\n");
		free(thr->io);
		free(thr->rc);
		free(thr);
		return NULL;
	}
	if (!CreatePipe(&thr->evt_pipe[0], &thr->evt_pipe[1], NULL, 0)) {
		LOG(Verbosity_VERBOSE, "Failed to create TermIO event pipe\n");
		CloseHandle(thr->evt_pipe_sem);
		free(thr->io);
		free(thr->rc);
		free(thr);
		return NULL;
	}

	// Start IO loop on thread
	pthread_create(&thr->thread, NULL, TermIOThread_loop, thr);

	return thr;
}

void TermIOThread_free(TermIOThread *thr) {
	// Stop thread
	ThreadRC_shutdown(thr->rc);
	pthread_join(thr->thread, NULL);
	
	// Free thread resources
	CloseHandle(thr->evt_pipe_sem);
	CloseHandle(thr->evt_pipe[0]);
	CloseHandle(thr->evt_pipe[1]);
	ThreadRC_free(thr->rc);
	TermIO_reset_and_free(thr->io);
	free(thr);
}

void TermIOThread_post_event(TermIOThread *thr, enum TermIO_Event_t event_type, uint64_t body_inline) {
	const TermIO_Event evt = {.event_type = event_type, .body_inline = body_inline};
	send_event(thr, &evt);
}
void TermIOThread_post_event2(TermIOThread *thr, enum TermIO_Event_t event_type, const void *body, const void *body2) {
	const TermIO_Event evt = {.event_type = event_type, .body = body, .body2 = body2};
	send_event(thr, &evt);
}

static void *TermIOThread_loop(void *thr__) {
	TermIOThread *thr = thr__;

	// Start in key input mode
	TermIO_set_input_mode(thr->io, InputMode_KEY);

	// Set up object array for polling
	// NOTE: WaitForMultipleObjects will return the lowest index corresponding to a signal object.
	// We want to be responsive to a shutdown signal (among other events), so we place evt_pipe_sem first in the array
	const HANDLE object_handles[] = {thr->evt_pipe_sem, stdin_handle};
	static const DWORD n_handles = sizeof(object_handles) / sizeof(object_handles[0]);

	while (ThreadRC_preloop(thr->rc)) {
		DWORD status = WaitForMultipleObjects(n_handles, object_handles, false, INFINITE);
		if (!(status >= WAIT_OBJECT_0 && status <= WAIT_OBJECT_0+(n_handles-1))) {
			LOG(Verbosity_NORMAL, "WaitForMultipleObjects failed. This should never happen\n");
			continue;
		}
		const DWORD signal_index = status - WAIT_OBJECT_0;

		// Handle TermIO_Event if one occured
		if (signal_index == 0) {
			TermIO_Event evt;
			if (!ReadFile(thr->evt_pipe[0], &evt, sizeof(TermIO_Event), NULL, NULL)) {
				LOG(Verbosity_VERBOSE, "Error: Failed to read TermIO_Event from pipe, possibly evt_pipe_sem desync\n");
				continue;
			}

			switch (evt.event_type) {
				case TermIO_THREADRC_WAKE:
					// goto preloop
					break;
				case TermIO_CHANGE_MODE:
					TermIO_set_input_mode(thr->io, evt.body_inline);
					break;
				case TermIO_TIMECODE:
					TermIO_update_timecode(thr->io, evt.body, evt.body2);
					break;
				case TermIO_PLAYBACK_STATE:
					TermIO_update_playback_state(thr->io, evt.body_inline);
					break;
				case TermIO_HISTORY_PREV:
					TermIO_shell_history_prev(thr->io);
					break;
				case TermIO_HISTORY_NEXT:
					TermIO_shell_history_next(thr->io);
					break;
				case TermIO_REPROMPT:
					TermIO_reprompt(thr->io);
					break;

				case TermIO_SHUTDOWN:
					fprintf(stderr, "YOU JUST SENT A DEPRECATED TermIO_Event, GET SENT TO DAVY JONES LOCKER\n");
					exit(1);
			}

			continue;
		}

		// Read keypress from stdin
		TermIO_handle_keypress(thr->io);
	}

	LOG(Verbosity_VERBOSE, "TermIO thread received cancel signal\n");
	return NULL;
}

#ifdef __WIN32
#undef stdin_handle
#undef stderr_handle
#endif
