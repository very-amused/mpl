#include <handleapi.h>
#include <io.h>
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

	HANDLE input; // Console input handle
	HANDLE output; // Console output handle

	// Event pipe handles (used to receive TermIO_Event's)
	HANDLE evt_pipe_rd, evt_pipe_wr;
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

	DWORD orig_console_input_mode;
	DWORD orig_console_output_mode;
	UINT orig_console_input_codepage;
	UINT orig_console_output_codepage;
};


// Update timecode and playback state in terminal output
static void write_playback_info(TermIOThread *thr, enum InputMode mode) {
	// Used to clear current line when refreshing timecode or playback state
	static const char CLEAR_LINE_VT100[] = "\033[2K\r";

	if (mode == InputMode_KEY) {
		char buf[255];
		const size_t buf_len = thr->playback_state == Queue_STOPPED
			? snprintf(buf, sizeof(buf), "(%s)", get_playback_icon(thr->playback_state))
			: snprintf(buf, sizeof(buf), "(%s) %s/%s", get_playback_icon(thr->playback_state), thr->timecode_buf, thr->duration_buf);
		WriteConsoleA(thr->output, CLEAR_LINE_VT100, sizeof(CLEAR_LINE_VT100)-1, NULL, NULL);
		WriteConsoleA(thr->output, buf, buf_len, NULL, NULL);
	} else if (mode == InputMode_SHELL) {
		static const char PROMPT_FMT[] = "(%s) %s/%s [mpl]$ ";
		static const char PROMPT_FMT_STOPPED[] = "(%s) [mpl]$ ";

		static char prompt[255];
		static size_t prompt_len = 0;
		const size_t new_prompt_len = thr->playback_state == Queue_STOPPED
			? snprintf(prompt, sizeof(prompt), PROMPT_FMT_STOPPED, get_playback_icon(thr->playback_state))
			: snprintf(prompt, sizeof(prompt), PROMPT_FMT, get_playback_icon(thr->playback_state), thr->timecode_buf, thr->duration_buf);
		if (new_prompt_len != prompt_len) {
			// Make readline aware of the prompt's length for redisplay purposes
			rl_set_prompt(prompt);
			rl_already_prompted = true;
		}

		WriteConsoleA(thr->output, CLEAR_LINE_VT100, sizeof(CLEAR_LINE_VT100)-1, NULL, NULL);
		WriteConsoleA(thr->output, prompt, prompt_len, NULL, NULL);
	}
}

static const char CONTINUE_IO_LOOP = -2;

// If mode == InputMode_KEY: get a single keypress input and return it (RETURN VALUE: char)
// If mode == InputMode_SHELL: handle shell input until mode change or shutdown (RETURN VALUE: int)
static int TermIOThread_get_input(TermIOThread *thr, enum InputMode mode) {
	// NOTE: WaitForMultipleObjects will return the lowest index corresponding to a signal object.
	// We want to be responsive to a shutdown signal (among other events), so we place evt_pipe_sem first in the array
	HANDLE object_handles[] = {thr->evt_pipe_sem, thr->input};
	static const DWORD n_handles = sizeof(object_handles) / sizeof(object_handles[0]);

	while (true) {
		// TODO: gate these logs behind a termio_debug flag
		LOG(Verbosity_DEBUG, "Blocking input on WaitForMultipleObjects...\n");
		DWORD status = WaitForMultipleObjects(n_handles, object_handles, false, INFINITE);
		LOG(Verbosity_DEBUG, "Done with WaitForMultipleObjects\n");


		// This is verbose but it's better to be API-compliant than rely on UB
		if (!(status >= WAIT_OBJECT_0 && status <= WAIT_OBJECT_0+(n_handles-1))) {
			LOG(Verbosity_NORMAL, "WaitForMultipleObjects failed. This should never happen\n");
			continue;
		}

		const DWORD signal_index = status - WAIT_OBJECT_0;

		// NOTE: semaphores are automatically decremented by WaitForMultipleObjects
		if (signal_index == 0) {
			// Read event from pipe
			TermIO_Event evt;
			bool ok = ReadFile(thr->evt_pipe_rd, &evt, sizeof(TermIO_Event), NULL, NULL);
			if (!ok) {
				LOG(Verbosity_VERBOSE, "Error: evt_pipe_sem desync'd from actual event pipe\n");
				continue;
			}

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
						write_playback_info(thr, mode);
					}
					break;

				case TermIO_PLAYBACK_STATE:
					{
						// Update playback state
						thr->playback_state = evt.body_inline;
						// Render timecode and playback state
						write_playback_info(thr, mode);
					}
					break;
			}
			continue;
		}

		DWORD n_read; // unused
		wchar_t c;
		ReadConsoleW(thr->input, &c, 1, &n_read, NULL);
		// We have to flush the input buffer to avoid the keypress event re-firing (yes, it is bad that we have to do this)
		FlushConsoleInputBuffer(thr->input);
		if (mode == InputMode_KEY) {
			return c;
		} else if (mode == InputMode_SHELL) {
			// Call any shell-enabled keybind bound to {c}
			if (KeybindMap_call_keybind(thr->keybinds, c, true) == Keybind_OK) {
				continue;
			}
			// If {c} is not bound to a shell keybind,
			// return it to the input stream and have readline process it
			rl_stuff_char(c);
			rl_callback_read_char();
		}
	}
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

	// Set codepage to UTF-8
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	// Start in key input mode
	TermIOThread_set_mode(thr, InputMode_KEY);

	while (true) {
		pthread_mutex_lock(&thr->mode_lock);
		const enum InputMode mode = thr->mode;
		pthread_mutex_unlock(&thr->mode_lock);

		int input = TermIOThread_get_input(thr, mode);
		if (input == EOF) {
			break;
		} else if (input == CONTINUE_IO_LOOP) {
			continue;
		}

		if (mode == InputMode_KEY) {
			Event evt = {.event_type = mpl_KEYPRESS, .body_size = sizeof(EventBody_Keypress), .body_inline = (EventBody_Keypress)input};
			EventSubQueue_send(thr->evt_sq, &evt, false);
		}
	}

	LOG(Verbosity_VERBOSE, "Terminal IO thread received cancel signal\n");
	CloseHandle(thr->evt_pipe_rd);
	// Reset terminal state
	SetConsoleCP(thr->orig_console_input_codepage);
	SetConsoleOutputCP(thr->orig_console_output_codepage);
	SetConsoleMode(thr->input, thr->orig_console_input_mode);
	SetConsoleMode(thr->output, thr->orig_console_output_mode);

	return NULL;
}

/* #endregion */

/* #region TermIO_Event send helper */

static void send_termio_evt(TermIOThread *thr, const TermIO_Event *evt) {
	WriteFile(thr->evt_pipe_wr, evt, sizeof(TermIO_Event), NULL, NULL);
	// This posts (increases) the semaphore,
	// ReleaseSemaphore is a poor choice of name
	ReleaseSemaphore(thr->evt_pipe_sem, 1, NULL);
}

/* #endregion */

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

	thr->input = GetStdHandle(STD_INPUT_HANDLE);
	thr->output = GetStdHandle(STD_ERROR_HANDLE);

	pthread_mutex_init(&thr->mode_lock, NULL);

	// Store original console mode (opts) and codepage (encoding)
	bool ok = true;
	ok &= GetConsoleMode(thr->input, &thr->orig_console_input_mode);
	ok &= GetConsoleMode(thr->output, &thr->orig_console_output_mode);
	if (!ok) {
		free(thr);
		return NULL;
	}
	thr->orig_console_input_codepage = GetConsoleCP();
	thr->orig_console_output_codepage = GetConsoleOutputCP();

	// Initialize event pipe and its associated semaphore
	ok = CreatePipe(&thr->evt_pipe_rd, &thr->evt_pipe_wr, NULL, 0);
	if (!ok) {
		free(thr);
		return NULL;
	}
	thr->evt_pipe_sem = CreateSemaphoreA(NULL, 0, -1u, NULL);
	if (!thr->evt_pipe_sem) {
		CloseHandle(thr->evt_pipe_wr);
		CloseHandle(thr->evt_pipe_rd);
		free(thr);
		return NULL;
	}

	// Start IO loop on thread
	if (!thr->thread) {
		CloseHandle(thr->evt_pipe_wr);
		CloseHandle(thr->evt_pipe_rd);
		free(thr);
		return NULL;
	}
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
	CloseHandle(thr->evt_pipe_wr); // evt_pipe_rd is closed in TermIOThread_loop
	free(thr);
}

void TermIOThread_set_mode(TermIOThread *thr, enum InputMode mode) {
	pthread_mutex_lock(&thr->mode_lock);

	thr->mode = mode;

	// Enable VT100 escape codes
	DWORD console_output_mode = thr->orig_console_output_mode;
	console_output_mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(thr->output, console_output_mode);

	// Input options we always turn off
	static const DWORD TERM_IN_OPTS_UNIVERSAL_MASK = ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);

	switch (thr->mode) {
		case InputMode_KEY:
			{
				if (thr->has_prompt) {
					// Clean up readline input
					rl_callback_handler_remove();
					rl_callback_userdata_ = NULL;
					// Advance to a blank line
					DWORD n_written;
					WriteConsoleA(thr->output, "\n", 1, &n_written, NULL);
				}
				// Tell the terminal we want to receive input without line buffering
				DWORD term_in_opts = thr->orig_console_input_mode;
				term_in_opts &= TERM_IN_OPTS_UNIVERSAL_MASK;
				term_in_opts &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
				SetConsoleMode(thr->input, term_in_opts);
			}
			break;

		case InputMode_SHELL:
			{
				// Tell the terminal we want to receive line buffered input
				DWORD term_in_opts = thr->orig_console_input_mode;
				term_in_opts &= TERM_IN_OPTS_UNIVERSAL_MASK;
				term_in_opts |= (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
				SetConsoleMode(thr->input, term_in_opts);
				// Advance to a blank line
				DWORD n_written;
				WriteConsoleA(thr->output, "\n", 1, &n_written, NULL);
				// Setup command history
				static bool history_initialized = false;
				if (!history_initialized) {
					using_history();
					history_initialized = true;
				}
				// Setup readline input
				rl_initialize();
				// FIXME: we need to use open_osfhandle and work with FILE * handles instead of HANDLE handles
			}
			break;
	}
}

void TermIOThread_update_timecode(TermIOThread *thr, const char *timecode_buf, const char *duration_buf);

void TermIOThread_update_playback_state(TermIOThread *thr, enum Queue_PLAYBACK_STATE playback_state);

/* #endregion */
