#include "input_thread.h"
#include "config/keybind/keybind_map.h"
#include "error.h"
#include "track_queue/state.h"
#include "ui/event_queue.h"
#include "ui/event.h"
#include "ui/icons.h"
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

struct TermIOThread {
	// EventSubQueue for sending input events
	EventSubQueue *evt_sq;
	// KeybindMap to implement shell_close keybind
	KeybindMap *keybinds;

	int input_fd; // FD to receive input on, usually stdin
	FILE *input;
	int output_fd; // FD to send output on, usually stderr
	FILE *output;

	// Pipe used to send TermIO_Event's which will wake up the thread
	int evt_pipe[2];

	// We need to keep track of these because
	// 1. when the timecode changes, we must already know the playback state
	// 2. when the playback state changes, we must already know the timecode
	char timecode_buf[255];
	char duration_buf[255];
	enum Queue_PLAYBACK_STATE playback_state;

	pthread_t thread;

	enum InputMode mode; // Input Mode
	bool has_prompt;
	pthread_mutex_t mode_lock; // Lock over {mode} and {has_prompt}

	struct termios orig_term_opts; // Original terminal state we reset to before exiting
};

// pthread routine for the InputThread
void *InputThread_loop(void *thr__);

TermIOThread *TermIOThread_new(EventQueue *eq, KeybindMap *keybinds) {
	TermIOThread *thr = malloc(sizeof(TermIOThread));
	CHECK_ALLOC(thr, NULL);
	memset(thr, 0, sizeof(TermIOThread));

	thr->evt_sq = EventQueue_connect(eq, 100);
	if (!thr->evt_sq) {
		free(thr);
		return NULL;
	}
	thr->keybinds = keybinds;

	thr->input_fd = STDIN_FILENO;
	thr->input = stdin;
	thr->output_fd = STDERR_FILENO;
	thr->output = stderr;

	pthread_mutex_init(&thr->mode_lock, NULL);

	// Store original terminal state
	tcgetattr(STDIN_FILENO, &thr->orig_term_opts);

	// Initialize event pipe for receiving TermIO events from the main thread
	pipe(thr->evt_pipe);

	// Start input thread
	pthread_create(&thr->thread, NULL, InputThread_loop, thr);

	return thr;
}

void TermIOThread_free(TermIOThread *thr) {
	// Send stop signal to thread
	const TermIO_Event shutdown_evt = {.event_type = TermIO_SHUTDOWN};
	write(thr->evt_pipe[1], &shutdown_evt, sizeof(TermIO_Event));

	// Join thread so we can safely free its resources
	pthread_join(thr->thread, NULL);

	// Free thread resources
	close(thr->evt_pipe[1]);
	free(thr);
}

static const char CONTINUE_IO_LOOP = -2;

// Used to clear current line when refreshing timecode or playback state
static const char CLEAR_LINE_VT100[] = "\033[2K\r";

// Block until a character is read from *thr or a cancel signal is received, returning EOF in the latter case.
//
// NOTE: assumes
// 1. thr->input_fd is on pollfds[0]
// 2. thr->evt_pipe[0] is on pollfds[1]
static char InputThread_getchar(TermIOThread *thr, struct pollfd pollfds[2]) {
	poll(pollfds, 2, -1);
	if (pollfds[1].revents & POLLIN) {
		TermIO_Event evt;
		read(pollfds[1].fd, &evt, sizeof(TermIO_Event));

		switch (evt.event_type) {
			case TermIO_SHUTDOWN:
				return EOF;
			case TermIO_CHANGE_MODE:
				return CONTINUE_IO_LOOP;

			case TermIO_TIMECODE:
				{
					// Update timecode
					strncpy(thr->timecode_buf, evt.body, sizeof(thr->timecode_buf)-1);
					strncpy(thr->duration_buf, evt.body2, sizeof(thr->duration_buf)-1);
					// Render timecode and playback state
#define WRITE_PLAYBACK_INFO() \
					thr->playback_state == Queue_STOPPED \
						? fprintf(thr->output, "%s", get_playback_icon(thr->playback_state)) \
						: fprintf(thr->output, "%s/%s %s", thr->timecode_buf, thr->duration_buf, get_playback_icon(thr->playback_state))

					fprintf(thr->output, CLEAR_LINE_VT100);
					WRITE_PLAYBACK_INFO();
				}
				break;

			case TermIO_PLAYBACK_STATE:
				{
					// Update playback state
					thr->playback_state = evt.body_inline;
					// Render timecode and playback state
					fprintf(thr->output, CLEAR_LINE_VT100);
					WRITE_PLAYBACK_INFO();

#undef WRITE_PLAYBACK_INFO
				}
				break;
		}

		return CONTINUE_IO_LOOP;
	}

	return fgetc(thr->input);
}

// Since readline's callback interface doesn't support passing userdata,
// we need a global pointer to the InputThread we notify when a line is ready
static TermIOThread *rl_thr_ptr_ = NULL;

static void write_playback_info(TermIOThread *thr) {
	static const char PROMPT_FMT[] = "%s/%s [mpl %s]$ ";
	static const char PROMPT_FMT_STOPPED[] = "[mpl %s]$ ";

	static char prompt[255];
	static size_t prompt_len = 0;
	const size_t new_prompt_len = thr->playback_state == Queue_STOPPED
		? snprintf(prompt, sizeof(prompt), PROMPT_FMT_STOPPED, get_playback_icon(thr->playback_state))
		: snprintf(prompt, sizeof(prompt), PROMPT_FMT, thr->timecode_buf, thr->duration_buf, get_playback_icon(thr->playback_state));
	if (new_prompt_len != prompt_len) {
		// Make readline aware of the prompt's length for redisplay purposes
		rl_set_prompt(prompt);
		rl_already_prompted = true;
	}

	fprintf(thr->output, CLEAR_LINE_VT100);
	fprintf(thr->output, "%s", prompt);
	rl_on_new_line_with_prompt();
	rl_redisplay();
}

// Enter a readline-powered shell, passing shell line events as needed.
//
// NOTE: assumes
// 1. thr->input_fd is on pollfds[0]
// 2. thr->evt_pipe[0] is on pollfds[1]
int InputThread_shell(TermIOThread *thr, struct pollfd pollfds[2]) {

	while (true) {
		poll(pollfds, 2, -1);
		if (pollfds[1].revents & POLLIN) {
			TermIO_Event evt;
			read(pollfds[1].fd, &evt, sizeof(TermIO_Event));

			switch (evt.event_type) {
				case TermIO_SHUTDOWN:
					return EOF;
				case TermIO_CHANGE_MODE:
					return CONTINUE_IO_LOOP;
				case TermIO_TIMECODE:
					// Update timecode
					strncpy(thr->timecode_buf, evt.body, sizeof(thr->timecode_buf)-1);
					strncpy(thr->duration_buf, evt.body2, sizeof(thr->duration_buf)-1);
					// Render timecode and playback state
					write_playback_info(thr);
					break;
				case TermIO_PLAYBACK_STATE:
					thr->playback_state = evt.body_inline;
					write_playback_info(thr);
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
	TermIOThread *thr = rl_thr_ptr_;

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
	TermIOThread *thr = thr__;

	// Start in key input mode
	TermIOThread_set_mode(thr, InputMode_KEY);

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
				} else if (input_key == CONTINUE_IO_LOOP) {
					continue;
				}
				Event evt = {.event_type = mpl_KEYPRESS, .body_size = sizeof(EventBody_Keypress), .body_inline = input_key};
				EventSubQueue_send(thr->evt_sq, &evt, false);
			}
			break;

		case InputMode_SHELL:
			{
				int status = InputThread_shell(thr, pollfds);
				if (status == EOF) {
					goto cancel;
				} else if (status == CONTINUE_IO_LOOP) {
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


void TermIOThread_set_mode(TermIOThread *thr, enum InputMode mode) {
	pthread_mutex_lock(&thr->mode_lock);

	thr->mode = mode;

	switch (thr->mode) {
	case InputMode_KEY:
		{
			if (thr->has_prompt) {
				// Clean up readline input
				rl_callback_handler_remove();
				rl_thr_ptr_ = NULL;
				// Advance to a blank line
				fprintf(thr->output, "\n");
			}
			// Tell the terminal we want to receive input without line buffering
			struct termios term_opts = thr->orig_term_opts;
			term_opts.c_lflag &= ~(ECHO | ICANON);
			tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
			thr->has_prompt = false;
		}
		break;
	
	case InputMode_SHELL:
		{
			// Tell the terminal we want to receive line buffered input
			struct termios term_opts = thr->orig_term_opts;
			term_opts.c_lflag |= (ECHO | ICANON);
			tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
			// Advance to a blank line
			fprintf(thr->output, "\n");
			// Setup readline input
			rl_initialize();
			rl_instream = thr->input;
			rl_outstream = thr->output;
			rl_thr_ptr_ = thr;
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

void TermIOThread_update_timecode(TermIOThread *thr, const char *timecode_buf, const char *duration_buf) {
	const TermIO_Event evt = {
		.event_type = TermIO_TIMECODE,
		.body = timecode_buf,
		.body2 = duration_buf
	};
	write(thr->evt_pipe[1], &evt, sizeof(TermIO_Event));
}

void TermIOThread_update_playback_state(TermIOThread *thr, enum Queue_PLAYBACK_STATE playback_state) {
	const TermIO_Event evt = {
		.event_type = TermIO_PLAYBACK_STATE,
		.body_inline = playback_state
	};
	write(thr->evt_pipe[1], &evt, sizeof(TermIO_Event));
}
