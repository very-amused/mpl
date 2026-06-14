#pragma once
#include "track_queue/state.h"
#include "ui/event_queue.h"
#include "config/keybind/keybind_map.h"
#include "ui/interface/cli/termio_events.h"

// A thread that handles passing input to an EventQueue without blocking
typedef struct TermIOThread TermIOThread;

// Allocate a new TermIOThread for use and immediately start receiving user input
TermIOThread *TermIOThread_new(EventQueue *eq, KeybindMap *keybinds);
// Join, deinitialize, and free a TermIOThread 
void TermIOThread_free(TermIOThread *thr);

// Post a TermIO_Event, causing the TermIO thread to do something
// (inline body, pass 0 if event has no body)
void TermIOThread_post_event(TermIOThread *thr, enum TermIO_Event_t event_type, uint64_t body_inline);
// Post a TermIO_Event with a pointer body
void TermIOThread_post_event2(TermIOThread *thr, enum TermIO_Event_t event_type, const void *body, const void *body2);

