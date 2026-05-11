#include "input_thread.h"
#include "ui/event_queue.h"
#include "ui/event.h"
#include "util/log.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <sys/poll.h>
#include <string.h>
#include <stddef.h>

struct InputThread {
	// EventSubQueue for sending input events
	EventSubQueue *evt_sq;

	int input_fd; // FD to receive input on, usually stdin
	FILE *input;
	// Pipe used to interrupt blocking and shutdown the input thread
	int shutdown_pipe[2];

	pthread_t thread;

	enum InputMode mode; // Input Mode
	pthread_mutex_t mode_lock; // Mutex guaranteeing the thread will stay in a given mode while held

	struct termios orig_term_opts; // Original terminal state we reset to before exiting
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

// Block until a line of text is read from *thr or a cancel signal is received, returning EOF in the latter case.
//
// NOTE: assumes thr->input_fd is on pollfds[0] and thr->cancel_pipe[0] is on pollfds[1]
int InputThread_getline(InputThread *thr, struct pollfd pollfds[2], char **line, size_t *line_len) {
	poll(pollfds, 2, -1);
	if (pollfds[1].revents & POLLIN) {
		return EOF;
	}

	// TODO: use readline callback interface instead of what's below

	*line = NULL;
	getline(line, line_len, thr->input);

	return 0;
}

void *InputThread_loop(void *thr__) {
	InputThread *thr = thr__;

	// Start in key input mode
	InputThread_set_mode(thr, InputMode_KEY);

	// Set up FD polling to make our blocking reads soft-cancelable
	struct pollfd pollfds[2];
	pollfds[0].fd = thr->input_fd;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = thr->shutdown_pipe[0];
	pollfds[1].events = POLLIN;

	while (true) {
		// Get user input
		pthread_mutex_lock(&thr->mode_lock);
		switch (thr->mode) {
		case InputMode_KEY:
			{
				EventBody_Keypress input_key = InputThread_getchar(thr, pollfds);
				if (input_key == EOF) {
					goto cancel;
				}
				Event evt = {.event_type = mpl_KEYPRESS, .body_size = sizeof(EventBody_Keypress), .body_inline = input_key};
				EventSubQueue_send(thr->evt_sq, &evt, false);
			}
			break;

		case InputMode_TEXT:
			{
				EventBody_InputLine line;
				size_t line_len;
				int status = InputThread_getline(thr, pollfds, &line, &line_len);
				if (status == EOF) {
					goto cancel;
				}
				Event evt = {.event_type = mpl_INPUT_LINE, .body_size = line_len, .body = line};
				EventSubQueue_send(thr->evt_sq, &evt, false);
			}
			break;
		}
		pthread_mutex_unlock(&thr->mode_lock);
	}

cancel:
	LOG(Verbosity_VERBOSE, "Input thread received cancel signal\n");
	close(thr->shutdown_pipe[0]);
	tcsetattr(STDIN_FILENO, TCSANOW, &thr->orig_term_opts);

	return NULL;
}

InputThread *InputThread_new(EventQueue *eq) {
	InputThread *thr = malloc(sizeof(InputThread));
	CHECK_ALLOC(thr, NULL);
	memset(thr, 0, sizeof(InputThread));

	thr->evt_sq = EventQueue_connect(eq, 100);
	if (!thr->evt_sq) {
		free(thr);
		return NULL;
	}

	thr->input_fd = STDIN_FILENO;
	thr->input = stdin;

	pthread_mutex_init(&thr->mode_lock, NULL);

	// Store original terminal state
	tcgetattr(STDIN_FILENO, &thr->orig_term_opts);

	// Initialize cancel pipe
	pipe(thr->shutdown_pipe);

	// Start input thread
	pthread_create(&thr->thread, NULL, InputThread_loop, thr);

	return thr;
}
void InputThread_free(InputThread *thr) {
	// Send stop signal to thread
	int cancel = 1;
	write(thr->shutdown_pipe[1], &cancel, sizeof(cancel));

	// Join thread so we can safely free its resources
	pthread_join(thr->thread, NULL);

	// Free thread resources
	close(thr->shutdown_pipe[1]);
	free(thr);
}

void InputThread_set_mode(InputThread *thr, enum InputMode mode) {
	pthread_mutex_lock(&thr->mode_lock);

	switch (mode) {
	case InputMode_KEY:
		{
			// Tell the terminal we want to receive input without line buffering
			struct termios term_opts = thr->orig_term_opts;
			term_opts.c_lflag &= ~(ECHO | ICANON);
			tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
		}
		break;
	
	case InputMode_TEXT:
		{
			// Tell the terminal we want to receive line buffered input
			struct termios term_opts = thr->orig_term_opts;
			term_opts.c_lflag &= (ECHO | ICANON);
			tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
			// TODO: setup readline input
		}
		break;
	}

	pthread_mutex_unlock(&thr->mode_lock);
}
