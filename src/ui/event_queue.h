#pragma once
#include "event.h"

// The central queue used by auxilliary threads to pass events to the main thread.
typedef struct EventQueue EventQueue;

// Allocate a new [EventQueue].
// Only one [EventQueue] should be used per MPL instance.
EventQueue *EventQueue_new();
// Free an EventQueue allocated using [EventQueue_new]
void EventQueue_free(EventQueue *eq);

// Send *evt on the EventQueue (*evt is copied).
// Returns 0 on success, nonzero on error
int EventQueue_send(EventQueue *eq, const Event *evt);
// Wait to receive n event on the EventQueue
// NOTE: May allocate evt->body, caller is responsible for freeing
// Returns 0 on success, nonzero on error
int EventQueue_recv(EventQueue *eq, Event *evt);

// Open a new connection to *eq with specific POSIX options.
EventQueue *EventQueue_connect(const EventQueue *eq1, int oflags);
