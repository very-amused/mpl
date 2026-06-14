#pragma once
#include "ui/interface/cli/termio_thread.h"

typedef struct TermIO TermIO;

TermIO *TermIO_new(EventQueue *eq, KeybindMap *keybinds);

// Update the IO input mode
void TermIO_set_input_mode(TermIO *io, enum InputMode mode);
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
