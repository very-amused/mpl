#pragma once
#include "ui/interface/cli/termio_thread.h"

typedef struct TermIO TermIO;

// Create a new TermIO struct
TermIO *TermIO_new(EventQueue *eq, KeybindMap *keybinds);
// Free a TermIO struct and reset terminal option state
void TermIO_reset_and_free(TermIO *io);

// Update the IO input mode
void TermIO_set_input_mode(TermIO *io, enum InputMode mode);

// Handle one keypress worth of input from stdin
// NOTE: this should be called after polling stdin so the caller knows we won't
// enter a non-cancelable block on stdin
void TermIO_handle_keypress(TermIO *io);

// Update the timecode displayed
void TermIO_update_timecode(TermIO *io, const char *timecode_buf, const char *duration_buf);
// Update the playback state displayed
void TermIO_update_playback_state(TermIO *io, enum Queue_PLAYBACK_STATE playback_state);
// Select the previous shell history entry
void TermIO_shell_history_prev(TermIO *io);
// Select the next shell history entry
void TermIO_shell_history_next(TermIO *io);
// Redraw the cached prompt
void TermIO_reprompt(TermIO *io);
