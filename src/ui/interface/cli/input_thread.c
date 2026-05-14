#include "input_thread.h"
#include "config/keybind/keybind_map.h"
#include "error.h"
#include "ui/event_queue.h"
#include "ui/event.h"
#include "util/log.h"
#include "termio_events.h"

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
#include <readline/readline.h>

struct InputThread {
	// EventSubQueue for sending input events
	EventSubQueue *evt_sq;
	// KeybindMap to implement shell_close keybind
	KeybindMap *keybinds;

	int input_fd; // FD to receive input on, usually stdin
	FILE *input;
	// Pipe used to send TermIO_Event's which will wake up the thread
	int evt_pipe[2];

	pthread_t thread;

	enum InputMode mode; // Input Mode
	bool has_prompt;
	pthread_mutex_t mode_lock; // Lock over {mode} and {has_prompt}

	struct termios orig_term_opts; // Original terminal state we reset to before exiting
};

// pthread routine for the InputThread
void *InputThread_loop(void *thr__);

InputThread *InputThread_new(EventQueue *eq, KeybindMap *keybinds) {
	InputThread *thr = malloc(sizeof(InputThread));
	CHECK_ALLOC(thr, NULL);
	memset(thr, 0, sizeof(InputThread));

	thr->evt_sq = EventQueue_connect(eq, 100);
	if (!thr->evt_sq) {
		free(thr);
		return NULL;
	}
	thr->keybinds = keybinds;

	thr->input_fd = STDIN_FILENO;
	thr->input = stdin;

	pthread_mutex_init(&thr->mode_lock, NULL);

	// Store original terminal state
	tcgetattr(STDIN_FILENO, &thr->orig_term_opts);

	// Initialize event pipe for receiving TermIO events from the main thread
	pipe(thr->evt_pipe);

	// Start input thread
	pthread_create(&thr->thread, NULL, InputThread_loop, thr);

	return thr;
}

void InputThread_free(InputThread *thr) {
	// Send stop signal to thread
	const TermIO_Event shutdown_evt = {.event_type = TermIO_SHUTDOWN};
	write(thr->evt_pipe[1], &shutdown_evt, sizeof(TermIO_Event));

	// Join thread so we can safely free its resources
	pthread_join(thr->thread, NULL);

	// Free thread resources
	close(thr->evt_pipe[1]);
	free(thr);
}

static const char CHANGE_MODE = -2;

// Block until a character is read from *thr or a cancel signal is received, returning EOF in the latter case.
//
// NOTE: assumes
// 1. thr->input_fd is on pollfds[0]
// 2. thr->evt_pipe[0] is on pollfds[1]
static char InputThread_getchar(InputThread *thr, struct pollfd pollfds[2]) {
	poll(pollfds, 2, -1);
	if (pollfds[1].revents & POLLIN) {
		TermIO_Event evt;
		read(pollfds[1].fd, &evt, sizeof(TermIO_Event));

		switch (evt.event_type) {
			case TermIO_SHUTDOWN:
				return EOF;
			case TermIO_CHANGE_MODE:
				return CHANGE_MODE;
			case TermIO_TIMECODE:
				// TODO: do what refresh_timecode in cli.c does
				break;
			case TermIO_PLAYBACK_STATE:
				// TODO: update playback state indicator
				// currently, we have no playback state indicator
				break;
		}
		// This just restarts the thread's loop
		return CHANGE_MODE;
	}

	return fgetc(thr->input);
}

// Since readline's callback interface doesn't support passing userdata,
// we need a global pointer to the InputThread we notify when a line is ready
static struct InputThread *rl_input_thread_ = NULL;

// Enter a readline-powered shell, passing shell line events as needed.
//
// NOTE: assumes
// 1. thr->input_fd is on pollfds[0]
// 2. thr->evt_pipe[0] is on pollfds[1]
int InputThread_shell(InputThread *thr, struct pollfd pollfds[2]) {
	while (true) {
		poll(pollfds, 2, -1);
		if (pollfds[1].revents & POLLIN) {
			TermIO_Event evt;
			read(pollfds[1].fd, &evt, sizeof(TermIO_Event));

			switch (evt.event_type) {
				case TermIO_SHUTDOWN:
					return EOF;
				case TermIO_CHANGE_MODE:
					return CHANGE_MODE;
				case TermIO_TIMECODE:
					// TODO: impl
					break;
				case TermIO_PLAYBACK_STATE:
					// TODO impl
					break;
			}
			continue;
		}

		char c = rl_getc(thr->input);
		// Call any shell-enabled keybind bound to {c}
		if (KeybindMap_call_shell_keybind(thr->keybinds, c) == Keybind_OK) {
			continue;
		}
		// If {c} is not bound to a shell-enabled keybind,
		// return it to the input stream and have readline process it
		rl_stuff_char(c);
		rl_callback_read_char();
	}
}

// Called when a line of input is ready
void rl_line_ready_cb_(char *line) {
	// Pass the line to be evaluated as shell syntax
	InputThread *thr = rl_input_thread_;

	// Send SHELL_CLOSE on EOF
	if (!line) {
		static const Event evt = {.event_type = mpl_SHELL_CLOSE, .body_size = 0};
		EventSubQueue_send(thr->evt_sq, &evt, false);
		return;
	}

	Event evt = {.event_type = mpl_INPUT_LINE, .body_size = strlen(line), .body = line};
	EventSubQueue_send(thr->evt_sq, &evt, false);
}

void *InputThread_loop(void *thr__) {
	InputThread *thr = thr__;

	// Start in key input mode
	InputThread_set_mode(thr, InputMode_KEY);

	// Set up FD polling to make our blocking reads soft-cancelable
	struct pollfd pollfds[2];
	pollfds[0].fd = thr->input_fd;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = thr->evt_pipe[0];
	pollfds[1].events = POLLIN;

	while (true) {
		// Get user input
		pthread_mutex_lock(&thr->mode_lock);
		enum InputMode mode = thr->mode;
		pthread_mutex_unlock(&thr->mode_lock);

		switch (mode) {
		case InputMode_KEY:
			{
				EventBody_Keypress input_key = InputThread_getchar(thr, pollfds);
				if (input_key == EOF) {
					goto cancel;
				} else if (input_key == CHANGE_MODE) {
					continue;
				}
				Event evt = {.event_type = mpl_KEYPRESS, .body_size = sizeof(EventBody_Keypress), .body_inline = input_key};
				EventSubQueue_send(thr->evt_sq, &evt, false);
			}
			break;

		case InputMode_TEXT:
			{
				int status = InputThread_shell(thr, pollfds);
				if (status == EOF) {
					goto cancel;
				} else if (status == CHANGE_MODE) {
					continue;
				}
			}
			break;
		}
	}

cancel:
	LOG(Verbosity_VERBOSE, "Input thread received cancel signal\n");
	close(thr->evt_pipe[0]);
	tcsetattr(STDIN_FILENO, TCSANOW, &thr->orig_term_opts);

	return NULL;
}


void InputThread_set_mode(InputThread *thr, enum InputMode mode) {
	pthread_mutex_lock(&thr->mode_lock);

	thr->mode = mode;

	switch (thr->mode) {
	case InputMode_KEY:
		{
			if (thr->has_prompt) {
				// Clean up readline input
				rl_callback_handler_remove();
				rl_input_thread_ = NULL;
				// Advance to a blank line
				printf("\n");
			}
			// Tell the terminal we want to receive input without line buffering
			struct termios term_opts = thr->orig_term_opts;
			term_opts.c_lflag &= ~(ECHO | ICANON);
			tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
			thr->has_prompt = false;
		}
		break;
	
	case InputMode_TEXT:
		{
			// Tell the terminal we want to receive line buffered input
			struct termios term_opts = thr->orig_term_opts;
			term_opts.c_lflag |= (ECHO | ICANON);
			tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
			// Setup readline input
			rl_initialize();
			rl_instream = thr->input;
			rl_input_thread_ = thr;
			rl_callback_handler_install("[mpl]$ ", rl_line_ready_cb_);
			thr->has_prompt = true;
		}
		break;
	}

	pthread_mutex_unlock(&thr->mode_lock);

	// Cancel anything blocking us from implementing the mode change
	const TermIO_Event evt = {.event_type = TermIO_CHANGE_MODE};
	write(thr->evt_pipe[1], &evt, sizeof(TermIO_Event));
}
