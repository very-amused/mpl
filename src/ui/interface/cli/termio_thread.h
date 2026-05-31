#pragma once
#include "track_queue/state.h"
#include "ui/event_queue.h"
#include "config/keybind/keybind_map.h"

// A thread that handles passing input to an EventQueue without blocking
typedef struct TermIOThread TermIOThread;

// InputMode tells the UI how to collect input data
enum InputMode {
	InputMode_KEY, // Get one key of input at a time without buffering
	InputMode_SHELL // Get shell input lines
};

// Allocate a new TermIOThread for use and immediately start receiving user input
TermIOThread *TermIOThread_new(EventQueue *eq, KeybindMap *keybinds);
// Join, deinitialize, and free a TermIOThread 
void TermIOThread_free(TermIOThread *thr);

// Update the IO mode
void TermIOThread_set_mode(TermIOThread *thr, enum InputMode mode);
// Update the timecode displayed
void TermIOThread_update_timecode(TermIOThread *thr, const char *timecode_buf, const char *duration_buf);
// Update the playback state displayed
void TermIOThread_update_playback_state(TermIOThread *thr, enum Queue_PLAYBACK_STATE playback_state);
// Select the previous shell history entry
void TermIOThread_history_prev(TermIOThread *thr);
// Select the next shell history entry
void TermIOThread_history_next(TermIOThread *thr);
