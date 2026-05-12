#include "input_thread.h"
#include "error.h"
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
#include <readline/readline.h>

struct InputThread {
	// EventSubQueue for sending input events
	EventSubQueue *evt_sq;

	int input_fd; // FD to receive input on, usually stdin
	FILE *input;
	// Pipe used to interrupt blocking and shutdown the input thread
	int shutdown_pipe[2];
	// Pipe used to interrupt blocking for changing the input mode
	int change_mode_pipe[2];
	// Whether we're polling (so it's safe to write to pipes)
	atomic_bool is_polling;

	pthread_t thread;

	enum InputMode mode; // Input Mode
	pthread_mutex_t mode_lock; // Mutex guaranteeing the thread will stay in a given mode while held

	struct termios orig_term_opts; // Original terminal state we reset to before exiting

	/* Readline handling for InputMode_TEXT */
	char *line; // Line of input read using readline
	pthread_cond_t line_cv; // Readline's char callback is finished, we can check if a new line is ready
	pthread_mutex_t line_lock; // Lock for rl_line
};

// pthread routine for the InputThread
void *InputThread_loop(void *thr__);

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
	pthread_mutex_init(&thr->line_lock, NULL);
	pthread_cond_init(&thr->line_cv, NULL);

	// Store original terminal state
	tcgetattr(STDIN_FILENO, &thr->orig_term_opts);

	// Initialize pipes
	pipe(thr->shutdown_pipe);
	pipe(thr->change_mode_pipe);

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

static const char CHANGE_MODE = -2;

// Block until a character is read from *thr or a cancel signal is received, returning EOF in the latter case.
//
// NOTE: assumes
// 1. thr->input_fd is on pollfds[0]
// 2. thr->cancel_pipe[0] is on pollfds[1]
// 3. thr->mode_change_pipe[0] is on pollfds[2]
static char InputThread_getchar(InputThread *thr, struct pollfd pollfds[3]) {
	poll(pollfds, 3, -1);
	if (pollfds[1].revents & POLLIN) {
		return EOF;
	} else if (pollfds[2].revents & POLLIN) {
		int tmp;
		read(thr->change_mode_pipe[0], &tmp, sizeof(tmp));
		return CHANGE_MODE;
	}

	return fgetc(thr->input);
}

// Since readline's callback interface doesn't support passing userdata,
// we need a global pointer to the InputThread we notify when a line is ready
static struct InputThread *rl_input_thread_ = NULL;

// Block until a line of text is read from *thr or a cancel signal is received, returning EOF in the latter case.
//
// NOTE: assumes
// 1. thr->input_fd is on pollfds[0]
// 2. thr->cancel_pipe[0] is on pollfds[1]
// 3. thr->mode_change_pipe[0] is on pollfds[2]
int InputThread_readline(InputThread *thr, struct pollfd pollfds[3], char **line, size_t *line_len) {
	while (true) {
		poll(pollfds, 3, -1);
		LOG(Verbosity_DEBUG, "Done polling in InputThread_readline\n");
		if (pollfds[1].revents & POLLIN) {
			return EOF;
		} else if (pollfds[2].revents & POLLIN) {
			int tmp;
			read(thr->change_mode_pipe[0], &tmp, sizeof(tmp));
			return CHANGE_MODE;
		}


		// Notify readline that a char is available
		LOG(Verbosity_DEBUG, "Key pressed in InputThread_readline\n");
		rl_callback_read_char();
		// Check if a line is available
		// FIXME: here's our problem, line_cv only gets posted if the char is LF,
		// otherwise we get stuck here. I don't think the callback API for readline is going to work for us
		pthread_mutex_lock(&thr->line_lock);
		pthread_cond_wait(&thr->line_cv, &thr->line_lock);
		if (!thr->line) {
			pthread_mutex_unlock(&thr->line_lock);
			continue;
		}
		LOG(Verbosity_VERBOSE, "thr->line = %s\n", thr->line);

		*line = thr->line;
		*line_len = strlen(*line);
		thr->line = NULL;
		pthread_mutex_unlock(&thr->line_lock);
		break;
	}

	return 0;
}

// Called when a line of input is ready
void rl_line_cb_(char *line) {
	InputThread *thr = rl_input_thread_;
	pthread_mutex_lock(&thr->line_lock);

	thr->line = line;
	pthread_cond_signal(&thr->line_cv);

	pthread_mutex_unlock(&thr->line_lock);
}

void *InputThread_loop(void *thr__) {
	InputThread *thr = thr__;

	// Start in key input mode
	InputThread_set_mode(thr, InputMode_KEY);

	// Set up FD polling to make our blocking reads soft-cancelable
	struct pollfd pollfds[3];
	pollfds[0].fd = thr->input_fd;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = thr->shutdown_pipe[0];
	pollfds[1].events = POLLIN;
	pollfds[2].fd = thr->change_mode_pipe[0];
	pollfds[2].events = POLLIN;

	atomic_store(&thr->is_polling, true);

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
				EventBody_InputLine line;
				size_t line_len;
				int status = InputThread_readline(thr, pollfds, &line, &line_len);
				if (status == EOF) {
					goto cancel;
				} else if (status == CHANGE_MODE) {
					continue;
				}
				Event evt = {.event_type = mpl_INPUT_LINE, .body_size = line_len, .body = line};
				EventSubQueue_send(thr->evt_sq, &evt, false);
			}
			break;
		}
	}

cancel:
	LOG(Verbosity_VERBOSE, "Input thread received cancel signal\n");
	close(thr->shutdown_pipe[0]);
	tcsetattr(STDIN_FILENO, TCSANOW, &thr->orig_term_opts);

	return NULL;
}


void InputThread_set_mode(InputThread *thr, enum InputMode mode) {
	pthread_mutex_lock(&thr->mode_lock);

	thr->mode = mode;

	switch (thr->mode) {
	case InputMode_KEY:
		{
			// Clean up readline input
			rl_callback_handler_remove();
			rl_input_thread_ = NULL;
			// Tell the terminal we want to receive input without line buffering
			struct termios term_opts = thr->orig_term_opts;
			term_opts.c_lflag &= ~(ECHO | ICANON);
			tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
		}
		break;
	
	case InputMode_TEXT:
		{
			// Tell the terminal we want to receive line buffered input
#if 0
			struct termios term_opts = thr->orig_term_opts;
			term_opts.c_lflag |= (ECHO | ICANON);
			tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
#endif
			// Setup readline input
			rl_initialize();
			rl_instream = thr->input;
			rl_resize_terminal();
			rl_input_thread_ = thr;
			rl_callback_handler_install("[mpl]$ ", rl_line_cb_);
		}
		break;
	}

	pthread_mutex_unlock(&thr->mode_lock);

	// Cancel anything blocking us from implementing the mode change
	if (atomic_load(&thr->is_polling)) {
		int cancel = 1;
		write(thr->change_mode_pipe[1], &cancel, sizeof(cancel));
	}
}
