#include "termio_thread.h"
#include "termio.h"
#include "config/keybind/keybind_map.h"
#include "error.h"
#include "ui/event_queue.h"
#include "util/log.h"
#include "termio_events.h"
#include "util/thread_rc.h"

#include <pthread.h>
#include <readline/keymaps.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <readline/readline.h>
#include <readline/history.h>

struct TermIOThread {
	pthread_t thread;
	// Handles thread shutdown
	ThreadRC *rc;

	// IO handling methods
	TermIO *io;

	// Pipe used to send TermIO_Event's to the thread
	int evt_pipe[2];
};

// pthread routine for the TermIOThread
static void *TermIOThread_loop(void *thr__);

static void TermIOThread_wake(void *ud) {
	TermIOThread *thr = ud;
	struct stat info;
	if (fstat(thr->evt_pipe[1], &info) != 0 || !S_ISFIFO(info.st_mode)) {
		return;
	}
	static const TermIO_Event wake_evt = {
		.event_type = TermIO_THREADRC_WAKE,
	};
	write(thr->evt_pipe[1], &wake_evt, sizeof(TermIO_Event));
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

	// Initialize event pipe for receiving TermIO events from the main thread
	pipe(thr->evt_pipe);

	// Start input thread
	pthread_create(&thr->thread, NULL, TermIOThread_loop, thr);

	return thr;
}

void TermIOThread_free(TermIOThread *thr) {
	// Stop thread
	ThreadRC_shutdown(thr->rc);
	pthread_join(thr->thread, NULL);

	// Free thread resources
	close(thr->evt_pipe[0]);
	close(thr->evt_pipe[1]);
	ThreadRC_free(thr->rc);
	TermIO_reset_and_free(thr->io);
	free(thr);
}

void TermIOThread_post_event(TermIOThread *thr, enum TermIO_Event_t event_type, uint64_t body_inline) {
	const TermIO_Event evt = {.event_type = event_type, .body_inline = body_inline};
	write(thr->evt_pipe[1], &evt, sizeof(TermIO_Event));
}

void TermIOThread_post_event2(TermIOThread *thr, enum TermIO_Event_t event_type, const void *body, const void *body2) {
	const TermIO_Event evt = {.event_type = event_type, .body = body, .body2 = body2};
	write(thr->evt_pipe[1], &evt, sizeof(TermIO_Event));
}

static void *TermIOThread_loop(void *thr__) {
	TermIOThread *thr = thr__;

	// Start in key input mode
	TermIO_set_input_mode(thr->io, InputMode_KEY);

	// Set up FD polling to make our blocking reads soft-cancelable
	struct pollfd pollfds[2];
	pollfds[0].fd = STDIN_FILENO;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = thr->evt_pipe[0];
	pollfds[1].events = POLLIN;

	while (ThreadRC_preloop(thr->rc)) {
		poll(pollfds, sizeof(pollfds)/sizeof(pollfds[0]), -1);

		// Handle TermIO_Event if one occured
		if (pollfds[1].revents & POLLIN) {
			TermIO_Event evt;
			read(pollfds[1].fd, &evt, sizeof(TermIO_Event));
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
					LOG(Verbosity_DEBUG, "Received TermIO_PLAYBACK_STATE event\n");
					TermIO_update_playback_state(thr->io, evt.body_inline);
					break;
				case TermIO_HISTORY_PREV:
					TermIO_shell_history_prev(thr->io);
					break;
				case TermIO_HISTORY_NEXT:
					TermIO_shell_history_next(thr->io);
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

	LOG(Verbosity_VERBOSE, "Input thread received cancel signal\n");
	return NULL;
}
