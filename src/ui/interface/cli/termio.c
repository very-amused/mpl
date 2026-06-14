#include "termio.h"
#include "ui/interface/cli/termio_thread.h"

#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>

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

	struct termios orig_term_opts; // Original terminal state we reset to when done
};

static void write_playback_info(TermIO *io);
// Process a line of shell input
static void shell_line_ready_cb(char *line);

void TermIO_set_input_mode(TermIO *io, enum InputMode mode) {
	if (mode == io->mode) {
		return;
	}

	static bool has_prompt = false;
	
	io->mode = mode;
	switch (io->mode) {
		case InputMode_KEY:
			{
				if (has_prompt) {
					// Clean up readline input
					rl_callback_handler_remove();
					shell_cb_userdata = NULL;
					// Advance to a blank line
					printf("\n");
				}
				// Tell the terminal we want to receive input without line buffering
				struct termios term_opts = io->orig_term_opts;
				term_opts.c_lflag &= ~(ECHO | ICANON);
				tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
				has_prompt = false;
			}

		case InputMode_SHELL:
			{
				// Tell the terminal we want to receive line buffered input
				struct termios term_opts = io->orig_term_opts;
				term_opts.c_lflag |= (ECHO | ICANON);
				tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);
				// Advance to a blank line
				printf("\n");
				// Setup command history
				static bool history_initialized = false;
				if (!history_initialized) {
					using_history();
					history_initialized = true;
				}
				// Setup readline input
				rl_initialize();
				shell_cb_userdata = io;
				rl_callback_handler_install("[mpl]$ ", shell_line_ready_cb);
				has_prompt = true;
			}
	}
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
	HIST_ENTRY *hist_prev = previous_history();
	if (hist_prev && hist_prev->line) {
		rl_replace_line(hist_prev->line, true);
		write_playback_info(io);
	}
}
void TermIO_shell_history_next(TermIO *io) {
	if (io->mode != InputMode_SHELL) {
		return;
	}
	HIST_ENTRY *hist_next = next_history();
	if (hist_next && hist_next->line) {
		rl_replace_line(hist_next->line, true);
		write_playback_info(io);
	}
}

void TermIO_reprompt(TermIO *io) {
	write_playback_info(io);
}
