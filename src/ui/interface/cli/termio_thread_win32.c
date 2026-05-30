#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <Windows.h>
#include <consoleapi.h>
#include <processenv.h>
#include <synchapi.h>
#include <consoleapi2.h>
#include <fileapi.h>
#include <wchar.h>
#include <winnls.h>
#include <handleapi.h>

#include "config/keybind/keybind_map.h"
#include "error.h"
#include "termio_thread.h"
#include "termio_events.h"
#include "track_queue/state.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "ui/icons.h"
#include "util/log.h"

/* #region IO handling */

struct TermIOThread {
	// EventSubQueue for sending input events to the main thread
	EventSubQueue *evt_sq;
	// KeybindMap to implement shbind keybinds
	KeybindMap *keybinds;

	HANDLE input_handle; // Console input handle
	HANDLE output_handle; // Console output handle
	// FILE * ptrs used for getline IO
	FILE *input;
	FILE *output;

	// Event pipe handles (used to receive TermIO_Event's)
	HANDLE evt_pipe[2]; // (evt_pipe[1] is the write end)
	HANDLE evt_pipe_sem; // Semaphore that counts events sent on evt_pipe

	// We need to keep track of these because
	// 1. when the timecode changes, we must already know the playback state
	// 2. when the playback state changes, we must already know the timecode
	char timecode_buf[255];
	char duration_buf[255];
	enum Queue_PLAYBACK_STATE playback_state;

	pthread_t thread;

	enum InputMode mode; // How we're processing input
	bool has_prompt;
	pthread_mutex_t mode_lock; // Lock over {mode} and {has_prompt}

	// Original terminal modes (opts)
	DWORD orig_term_mode_input;
	DWORD orig_term_mode_output;
	// Original terminal codepages (encodings)
	UINT orig_term_codepage_input;
	UINT orig_term_codepage_output;
};

static const char CONTINUE_IO_LOOP = -2;

// Block until a character is read from *thr or a cancel signal is received, returning EOF in the latter case
static char TermIOThread_getchar(TermIOThread *thr) {
	// NOTE: WaitForMultipleObjects will return the lowest index corresponding to a signal object.
	// We want to be responsive to a shutdown signal (among other events), so we place evt_pipe_sem first in the array
	HANDLE object_handles[] = {thr->evt_pipe_sem, thr->input_handle};
	static const DWORD n_handles = sizeof(object_handles) / sizeof(object_handles[0]);

	DWORD status = WaitForMultipleObjects(n_handles, object_handles, false, INFINITE);
	if (!(status >= WAIT_OBJECT_0 && status <= WAIT_OBJECT_0+(n_handles-1))) {
		LOG(Verbosity_NORMAL, "WaitForMultipleObjects failed. This should never happen\n");
		return CONTINUE_IO_LOOP;
	}
	const DWORD signal_index = status - WAIT_OBJECT_0;

	if (signal_index == 0) {
		// TermIO_Event ready on pipe
		TermIO_Event evt;
		if (!ReadFile(thr->evt_pipe[0], &evt, sizeof(TermIO_Event), NULL, NULL)) {
			LOG(Verbosity_VERBOSE, "Error: Failed to read TermIO_Event from pipe, possibly evt_pipe_sem desync\n");
			return CONTINUE_IO_LOOP;
		}

		switch (evt.event_type) {
			case TermIO_SHUTDOWN:
				return EOF;
			case TermIO_CHANGE_MODE:
				return CONTINUE_IO_LOOP;
			default:
				return CONTINUE_IO_LOOP;
		}
		return CONTINUE_IO_LOOP;
	}

	return fgetc(thr->input);
}

// Enter a readline-powered shell, passing shell line events as needed
static int TermIOThread_shell(TermIOThread *thr) {
	HANDLE object_handles[] = {thr->evt_pipe_sem, thr->input_handle};
	static const DWORD n_handles = sizeof(object_handles) / sizeof(object_handles[0]);

	while (true) {
		DWORD status = WaitForMultipleObjects(n_handles, object_handles, false, INFINITE);
		if (!(status >= WAIT_OBJECT_0 && status <= WAIT_OBJECT_0+(n_handles-1))) {
			LOG(Verbosity_NORMAL, "WaitForMultipleObjects failed. This should never happen\n");
			continue;
		}
		const DWORD signal_index = status - WAIT_OBJECT_0;

		if (signal_index == 0) {
			// TermIO_Event ready on pipe
			TermIO_Event evt;
			if (!ReadFile(thr->evt_pipe[0], &evt, sizeof(TermIO_Event), NULL, NULL)) {
				LOG(Verbosity_VERBOSE, "Error: Failed to read TermIO_Event from pipe, possibly evt_pipe_sem desync\n");
				continue;
			}

			switch (evt.event_type) {
				case TermIO_SHUTDOWN:
					return EOF;
				case TermIO_CHANGE_MODE:
					return CONTINUE_IO_LOOP;
				default:
					continue;
			}
			continue; // continue loop
		}

		char c = rl_getc(thr->input);
		// Call any shell-enabled keybind bound to {c}
		// FIXME: we NEED mod key support after v0.5.0
		if (KeybindMap_call_keybind(thr->keybinds, c, true) == Keybind_OK) {
			continue;
		}
		// If {c} is not bound to a shell-enabled keybind,
		// return it to the input stream and have readline process it
		rl_stuff_char(c);
		rl_callback_read_char();
	}

	return CONTINUE_IO_LOOP;
}

static TermIOThread *rl_callback_userdata_ = NULL;

static void rl_line_ready_cb_(char *line) {
	TermIOThread *thr = rl_callback_userdata_;

	// Send SHELL_CLOSE on EOF
	if (!line) {
		const Event evt = {.event_type = mpl_SHELL_CLOSE, .body_size = 0};
		EventSubQueue_send(thr->evt_sq, &evt, false);
		return;
	}

	add_history(line);
	Event evt = {.event_type = mpl_INPUT_LINE, .body_size = strlen(line), .body = line};
	EventSubQueue_send(thr->evt_sq, &evt, false);
}

static void *TermIOThread_loop(void *thr__) {
	TermIOThread *thr = thr__;

	// Use UTF-8 for terminal IO
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	// Start in key input mode
	TermIOThread_set_mode(thr, InputMode_KEY);

	while (true) {
		pthread_mutex_lock(&thr->mode_lock);
		const enum InputMode mode = thr->mode;
		pthread_mutex_unlock(&thr->mode_lock);

		switch (mode) {
		case InputMode_KEY:
			{
				EventBody_Keypress input_key = TermIOThread_getchar(thr);
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
				int status = TermIOThread_shell(thr);
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
	LOG(Verbosity_VERBOSE, "Terminal IO thread received cancel signal\n");
	CloseHandle(thr->evt_pipe[0]);
	// Reset terminal state
	SetConsoleMode(thr->input_handle, thr->orig_term_mode_input);
	SetConsoleMode(thr->output_handle, thr->orig_term_mode_output);
	SetConsoleCP(thr->orig_term_codepage_input);
	SetConsoleOutputCP(thr->orig_term_codepage_output);

	return NULL;
}

/* #endregion */

static void send_termio_evt(TermIOThread *thr, const TermIO_Event *evt) {
	WriteFile(thr->evt_pipe[1], evt, sizeof(TermIO_Event), NULL, NULL);
	// This posts (increases) the semaphore,
	// ReleaseSemaphore is a poor choice of name
	ReleaseSemaphore(thr->evt_pipe_sem, 1, NULL);
}

/* #region TermIOThread implementation */

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

	// Set up IO handles
	thr->input_handle = GetStdHandle(STD_INPUT_HANDLE);
	thr->input = stdin;
	thr->output_handle = GetStdHandle(STD_ERROR_HANDLE);
	thr->output = stderr;

	pthread_mutex_init(&thr->mode_lock, NULL);

	// Store original console mode (opts) and codepage (encoding)
	if (!GetConsoleMode(thr->input_handle, &thr->orig_term_mode_input)) {
		free(thr);
		return NULL;
	}
	if (!GetConsoleMode(thr->output_handle, &thr->orig_term_mode_output)) {
		free(thr);
		return NULL;
	}
	thr->orig_term_codepage_input = GetConsoleCP();
	thr->orig_term_codepage_output = GetConsoleOutputCP();

	// Initialize event pipe and its associated semaphore
	thr->evt_pipe_sem = CreateSemaphoreA(NULL, 0, -1u >> 1, NULL);
	if (!thr->evt_pipe_sem) {
		LOG(Verbosity_DEBUG, "failed here\n");
		free(thr);
		return NULL;
	}
	if (!CreatePipe(&thr->evt_pipe[0], &thr->evt_pipe[1], NULL, 0)) {
		CloseHandle(thr->evt_pipe_sem);
		free(thr);
		return NULL;
	}

	// Start IO loop on thread
	pthread_create(&thr->thread, NULL, TermIOThread_loop, thr);

	return thr;
}

void TermIOThread_free(TermIOThread *thr) {
	// shutdown thread
	const TermIO_Event shutdown_evt = {.event_type = TermIO_SHUTDOWN};
	send_termio_evt(thr, &shutdown_evt);
	pthread_join(thr->thread, NULL);

	// Free thread resources
	CloseHandle(thr->evt_pipe_sem);
	CloseHandle(thr->evt_pipe[1]); // evt_pipe[0] is closed in TermIOThread_loop
	free(thr);
}

void TermIOThread_set_mode(TermIOThread *thr, enum InputMode mode) {
	pthread_mutex_lock(&thr->mode_lock);

	thr->mode = mode;

	// Enable VT100 escape codes
	DWORD console_output_mode = thr->orig_term_mode_output;
	console_output_mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(thr->output_handle, console_output_mode);

	// Input options we always turn off
	static const DWORD TERM_IN_OPTS_MASK = ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);

	switch (thr->mode) {
		case InputMode_KEY:
			{
				if (thr->has_prompt) {
					// Clean up readline input
					rl_callback_handler_remove();
					rl_callback_userdata_ = NULL;
					// Advance to a blank line
					fprintf(thr->output, "\n");
				}
				// Tell the terminal we want to receive input without line buffering
				DWORD term_in_opts = thr->orig_term_mode_input;
				term_in_opts &= TERM_IN_OPTS_MASK;
				term_in_opts &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
				SetConsoleMode(thr->input_handle, term_in_opts);
			}
			break;

		case InputMode_SHELL:
			{
				// Tell the terminal we want to receive line buffered input
				DWORD term_in_opts = thr->orig_term_mode_input;
				term_in_opts &= TERM_IN_OPTS_MASK;
				term_in_opts |= (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
				SetConsoleMode(thr->input_handle, term_in_opts);
				// Advance to a blank line
				fprintf(thr->output, "\n");
				// Setup command history
				static bool history_initialized = false;
				if (!history_initialized) {
					using_history();
					history_initialized = true;
				}
				// Setup readline input
				rl_initialize();
				rl_instream = thr->input;
				rl_outstream = thr->output;
				rl_callback_userdata_ = thr;
				rl_callback_handler_install("$[mpl] ", rl_line_ready_cb_);
				thr->has_prompt = true;
			}
			break;
	}

	pthread_mutex_unlock(&thr->mode_lock);

	// Cancel anything blocking us from implementing the mode change
	const TermIO_Event evt = {.event_type = TermIO_CHANGE_MODE};
	send_termio_evt(thr, &evt);
}

void TermIOThread_update_timecode(TermIOThread *thr, const char *timecode_buf, const char *duration_buf) {
	const TermIO_Event evt = {
		.event_type = TermIO_TIMECODE,
		.body = timecode_buf,
		.body2 = duration_buf
	};
	send_termio_evt(thr, &evt);
}

void TermIOThread_update_playback_state(TermIOThread *thr, enum Queue_PLAYBACK_STATE playback_state) {
	const TermIO_Event evt = {
		.event_type = TermIO_PLAYBACK_STATE,
		.body_inline = playback_state
	};
	send_termio_evt(thr, &evt);
}

/* #endregion */
