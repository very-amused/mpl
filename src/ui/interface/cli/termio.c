#include "termio.h"
#include "config/keybind/keybind_map.h"
#include "error.h"
#include "track_queue/state.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "ui/icons.h"
#include "util/log.h"

#include <libavutil/mem.h>
#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __unix__
#include <termios.h>
#elif defined(__WIN32)
#include <consoleapi.h>
#include <processenv.h>
#include <Windows.h>

#define stdin_handle GetStdHandle(STD_INPUT_HANDLE)
#define stderr_handle GetStdHandle(STD_ERROR_HANDLE)
#endif

static TermIO *shell_cb_userdata; // userdata for readline shell callback

struct TermIO {
	enum InputMode mode;

	// EventSubQueue for sending input events
	EventSubQueue *evt_sq;
	// KeybindMap to implement shell keybinds
	KeybindMap *keybinds;

	// We need to keep track of these because
	// 1. when the timecode changes, we must already know the playback state
	// 2. when the playback state changes, we must already know the timecode
	char timecode_buf[255];
	char duration_buf[255];
	enum Queue_PLAYBACK_STATE playback_state;

	// Active line buffer for saving the line being typed when navigating history
	char *shell_active_line_buf;
	unsigned int shell_active_line_buf_max;
	bool shell_active_line_buf_valid; // used to invalite the active line buffer when a shell session closes

#ifdef __unix__
	struct termios orig_term_opts; // Original terminal state we reset to when done
#elif defined(__WIN32)
	// Original terminal modes (opts)
	DWORD orig_term_mode_input;
	DWORD orig_term_mode_output;
#endif
};

TermIO *TermIO_new(EventQueue *eq, KeybindMap *keybinds) {
	TermIO *io = malloc(sizeof(TermIO));
	CHECK_ALLOC(io, NULL);
	memset(io, 0, sizeof(TermIO));

	io->evt_sq = EventQueue_connect(eq, 100);
	if (!io->evt_sq) {
		free(io);
		return NULL;
	}
	io->keybinds = keybinds;

	// Store original terminal state so we can reset to it during freeing
#ifdef __unix__
	tcgetattr(STDIN_FILENO, &io->orig_term_opts);
#elif defined(__WIN32)
	GetConsoleMode(stdin_handle, &io->orig_term_mode_input);
	GetConsoleMode(stderr_handle, &io->orig_term_mode_output);
#endif

	io->playback_state = -1;

	return io;
}

void TermIO_reset_and_free(TermIO *io) {
	// Free active line buffer
	free(io->shell_active_line_buf);
	// Reset terminal state
#ifdef __unix__
	tcsetattr(STDIN_FILENO, TCSANOW, &io->orig_term_opts);
#elif defined(__WIN32)
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), io->orig_term_mode_input);
	SetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), io->orig_term_mode_output);
#endif
	// Free TermIO struct
	free(io);
}

// Write playback info taking input mode into account
// (i.e in shell mode this updates the prompt, in key mode this redraws the status line)
static void write_playback_info(TermIO *io);
// Process a line of shell input
static void shell_line_ready_cb(char *line);

void TermIO_handle_keypress(TermIO *io) {
	typedef int (*getc_fn)(FILE *);
	getc_fn get_char;
	switch (io->mode) {
		case InputMode_KEY:
			get_char = fgetc;
			break;
		case InputMode_SHELL:
			get_char = rl_getc;
	}

	// Read keypress from stdin
	// TODO: parse escape sequences
	// TODO: support mod keys
	char c = get_char(stdin);

	// Handle keypress
	switch (io->mode) {
		case InputMode_KEY:
			{
				// Send mpl_KEYPRESS event to main thread
				Event evt = {.event_type = mpl_KEYPRESS, .body_size = sizeof(EventBody_Keypress), .body_inline = c};
				LOG(Verbosity_DEBUG, "Sending mpl_KEYPRESS event\n");
				EventSubQueue_send(io->evt_sq, &evt, false);
			}
			break;

		case InputMode_SHELL:
			{
				// Call any shell-enabled keybind bound to {c}
				if (KeybindMap_call_keybind(io->keybinds, c, true) == Keybind_OK) {
					return;
				}
				// If {c} is not bound to a shell-enabled keybind,
				// return it to the input stream and have readline process it
				rl_stuff_char(c);
				// Trigger callback if we just terminated a line of input
				rl_callback_read_char();
			}
			break;
	}
}

void TermIO_set_input_mode(TermIO *io, enum InputMode mode) {
	static bool has_prompt = false;

#ifdef __WIN32
	// Input options we always turn off
	static const DWORD CONSOLE_INPUT_MODE_MASK = ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);

	// Enable VT100 escape codes
	DWORD console_output_mode = io->orig_term_mode_output;
	console_output_mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(stderr_handle, console_output_mode);
#endif
	
	io->mode = mode;
	switch (io->mode) {
		case InputMode_KEY:
			{
				if (has_prompt) {
					// Clean up readline input
					rl_callback_handler_remove();
					shell_cb_userdata = NULL;
					// Advance to a blank line
					fprintf(stderr, "\n");
				}
				// Tell the terminal we want to receive input without line buffering
#ifdef __unix__
				struct termios term_opts = io->orig_term_opts;
				term_opts.c_lflag &= ~(ECHO | ICANON);
				tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
#elif defined(__WIN32)
				DWORD console_input_mode = io->orig_term_mode_input & CONSOLE_INPUT_MODE_MASK;
				console_input_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
				SetConsoleMode(stdin_handle, console_input_mode);
#endif
				has_prompt = false;
			}
			break;

		case InputMode_SHELL:
			{
				// Tell the terminal we want to receive line buffered input
#ifdef __unix__
				struct termios term_opts = io->orig_term_opts;
				term_opts.c_lflag |= (ECHO | ICANON);
				tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
#elif defined(__WIN32)
				DWORD console_input_mode = io->orig_term_mode_input & CONSOLE_INPUT_MODE_MASK;
				console_input_mode |= (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
				SetConsoleMode(stdin_handle, console_input_mode);
#endif
				// Advance to a blank line
				fprintf(stderr, "\n");
				// Setup command history
				static bool history_initialized = false;
				if (!history_initialized) {
					using_history();
					history_initialized = true;
				}
				// Invalidate active line buffer
				io->shell_active_line_buf_valid = false;
				// Setup readline input
				rl_initialize();
				shell_cb_userdata = io;
				rl_callback_handler_install("[mpl]$ ", shell_line_ready_cb);
				has_prompt = true;
			}
			break;
	}

	// Reprompt
	write_playback_info(io);
}

void TermIO_update_timecode(TermIO *io, const char *timecode_buf, const char *duration_buf) {
	strncpy(io->timecode_buf, timecode_buf, sizeof(io->timecode_buf)-1);
	strncpy(io->duration_buf, duration_buf, sizeof(io->duration_buf)-1);
	// Render playback info
	write_playback_info(io);
}


void TermIO_update_playback_state(TermIO *io, enum Queue_PLAYBACK_STATE playback_state) {
	io->playback_state = playback_state;	
	write_playback_info(io);
}

void TermIO_shell_history_prev(TermIO *io) {
	if (io->mode != InputMode_SHELL) {
		return;
	}
	// Remember the line buffer's contents so we can go back to it
	// in shell_history_next once we hit the end of the history list
	if (!current_history()) {
		size_t rl_line_buf_len = strlen(rl_line_buffer);
		av_fast_malloc(&io->shell_active_line_buf, &io->shell_active_line_buf_max, rl_line_buf_len+1);
		strncpy(io->shell_active_line_buf, rl_line_buffer, io->shell_active_line_buf_max);
		io->shell_active_line_buf_valid = true;
		LOG(Verbosity_DEBUG, "Saving active line buffer @ %p\n", io->shell_active_line_buf);
	}

	// Move to prev history entry and replace line buffer
	HIST_ENTRY *hist_prev = previous_history();
	if (hist_prev && hist_prev->line) {
		rl_replace_line(hist_prev->line, true);
		rl_point = strlen(hist_prev->line); // move cursor to end of line
		write_playback_info(io);
	}
}
void TermIO_shell_history_next(TermIO *io) {
	if (io->mode != InputMode_SHELL) {
		return;
	}

	HIST_ENTRY *hist_next = next_history();
	if (hist_next && hist_next->line) {
		// Replace line with next history entry
		rl_replace_line(hist_next->line, true);
		rl_point = strlen(hist_next->line); // move cursor to end of line
		write_playback_info(io);
	} else if (io->shell_active_line_buf && io->shell_active_line_buf_valid) {
		LOG(Verbosity_DEBUG, "Restoring active line buffer\n");
		// We've hit the end of the history list, restore the line being typed before we started looking around in history
		rl_replace_line(io->shell_active_line_buf, true);
		rl_point = strlen(io->shell_active_line_buf);
		io->shell_active_line_buf_valid = false;
		write_playback_info(io);
	}
}

void TermIO_reprompt(TermIO *io) {
	write_playback_info(io);
}

static void write_playback_info(TermIO *io) {
	static const char CLEAR_LINE_VT100[] = "\033[2K\r";

	// Don't prompt before playback state is initialized
	if (io->playback_state == -1)  {
		return;
	}

	switch (io->mode) {
		case InputMode_KEY:
			fprintf(stderr, CLEAR_LINE_VT100);
			io->playback_state == Queue_STOPPED
				? fprintf(stderr, "(%s)", get_playback_icon(io->playback_state))
				: fprintf(stderr, "(%s) %s/%s", get_playback_icon(io->playback_state), io->timecode_buf, io->duration_buf);
			break;

		case InputMode_SHELL:
			{
				static const char PROMPT_FMT[] = "(%s) %s/%s [mpl]$ ";
				static const char PROMPT_FMT_STOPPED[] = "(%s) [mpl]$ ";

				static char prompt[255];
				static size_t prompt_len = 0;
				const size_t new_prompt_len = io->playback_state == Queue_STOPPED
					? snprintf(prompt, sizeof(prompt), PROMPT_FMT_STOPPED, get_playback_icon(io->playback_state))
					: snprintf(prompt, sizeof(prompt), PROMPT_FMT, get_playback_icon(io->playback_state), io->timecode_buf, io->duration_buf);
				if (new_prompt_len != prompt_len) {
					// Make readline aware of the prompt's length for redisplay purposes
					rl_set_prompt(prompt);
					rl_already_prompted = true;
				}

				fprintf(stderr, CLEAR_LINE_VT100);
				fprintf(stderr, "%s", prompt);
				rl_on_new_line_with_prompt();
				rl_redisplay();
			}
			break;
	}
}

static void shell_line_ready_cb(char *line) {
	TermIO *io = shell_cb_userdata;

	// Invalidate active line buffer
	if (io->shell_active_line_buf && io->shell_active_line_buf_valid) {
		io->shell_active_line_buf_valid = false;
	}

	// Send SHELL_CLOSE on EOF
	if (!line) {
		static const Event evt = {.event_type = mpl_SHELL_CLOSE, .body_size = 0};
		EventSubQueue_send(io->evt_sq, &evt, false);
		return;
	}

	// Add line to history and send mpl_INPUT_LINE
	add_history(line);
	Event evt = {.event_type = mpl_INPUT_LINE, .body_size = strlen(line), .body = line};
	EventSubQueue_send(io->evt_sq, &evt, false);
}

#ifdef __WIN32
#undef stdin_handle
#undef stderr_handle
#endif
