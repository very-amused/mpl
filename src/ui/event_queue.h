#pragma once
#include "event.h"

// The central queue used by auxilliary threads to pass events to the main thread.
// NOTE: this handle is used by the main thread to read events.
// Auxilliary threads must call EventQueue_connect to obtain an EventSubQueue for writing events.
typedef struct EventQueue EventQueue;

// A subqueue used by a thread to send events to the main thread's EventQueue.
typedef struct EventSubQueue EventSubQueue;

// Allocate a new [EventQueue].
// Only one [EventQueue] should be used per MPL instance.
EventQueue *EventQueue_new();
// Free an EventQueue allocated using [EventQueue_new]
void EventQueue_free(EventQueue *eq);

// Send *evt on the EventQueue (*evt is copied).
// Returns 0 on success, nonzero on error
// WARN: DEPRECATED in favor of EventSubQueue_send.
int EventQueue_send(EventQueue *eq, const Event *evt);
// Wait to receive an event on the EventQueue
// NOTE: May allocate evt->body, caller is responsible for freeing
// Returns 0 on success, nonzero on error
int EventQueue_recv(EventQueue *eq, Event *evt);

// Open a new subqueue that feeds events to *eq.
// WARN: DEPRECATED in favor of EventQueue_connect
EventQueue *EventQueue_connect_legacy(const EventQueue *eq1, int oflags);
