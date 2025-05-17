#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <sys/poll.h>

#include "input_thread.h"
#include "event_queue.h"
#include "ui/event.h"
#include "util/log.h"

struct InputThread {
	// EventQueue for sending input events
	EventQueue *eq;

	int input_fd; // FD to receive input on, usually stdin
	FILE *input;
	// Pipe used to interrupt blocking and shutdown the input thread
	int shutdown_pipe[2];

	pthread_t thread;
	_Atomic(enum InputMode) mode; // Input Mode
};

// Block until a character is read from *thr or a cancel signal is received, returning EOF in the latter case.
//
// NOTE: assumes thr->input_fd is on pollfds[0] and thr->cancel_pipe[0] is on pollfds[1]
static char InputThread_getchar(InputThread *thr, struct pollfd pollfds[2]) {
	poll(pollfds, 2, -1);
	if (pollfds[1].revents & POLLIN) {
		return EOF;
	}
	return fgetc(thr->input);
}

void *InputThread_loop(void *thr__) {
	InputThread *thr = thr__;

	// Set up FD polling to make our blocking reads cancelable
	struct pollfd pollfds[2];
	pollfds[0].fd = thr->input_fd;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = thr->shutdown_pipe[0];
	pollfds[1].events = POLLIN;

	// Tell the terminal we want to receive input without line buffering
	struct termios term_opts;
	tcgetattr(STDIN_FILENO, &term_opts);
	const struct termios orig_term_opts = term_opts;
	term_opts.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);

	while (true) {

		// Get user input
		const enum InputMode mode = atomic_load(&thr->mode);
		switch (mode) {
		case InputMode_KEY:
		{
			EventBody_Keypress input_key = InputThread_getchar(thr, pollfds);
			if (input_key == EOF) {
				goto cancel;
			}
			Event evt = {.event_type = mpl_KEYPRESS, .body_size = sizeof(EventBody_Keypress), .body_inline = input_key};
			EventQueue_send(thr->eq, &evt);
			break;
		}
		case InputMode_TEXT:
			fprintf(stderr, "InputMode_TEXT is not yet implemented!\n");
			continue;
		}
	}

cancel:
	LOG(Verbosity_VERBOSE, "Input thread received cancel signal\n");
	close(thr->shutdown_pipe[0]);
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_opts);
	return NULL;
}

InputThread *InputThread_new(EventQueue *eq) {
	InputThread *thr = malloc(sizeof(InputThread));
	thr->eq = eq;
	thr->mode = InputMode_KEY;
	thr->input_fd = STDIN_FILENO;
	thr->input = stdin;


	// Initialize cancel pipe
	pipe(thr->shutdown_pipe);
	// Start input thread
	pthread_create(&thr->thread, NULL, InputThread_loop, thr);

	return thr;
}
void InputThread_free(InputThread *thr) {
	// Stop thread
	int cancel = 1;
	write(thr->shutdown_pipe[1], &cancel, sizeof(cancel));
	pthread_join(thr->thread, NULL);
	close(thr->shutdown_pipe[1]);
	free(thr);
}

void InputThread_set_mode(InputThread *thr, enum InputMode mode) {
	atomic_store(&thr->mode, mode);
}
