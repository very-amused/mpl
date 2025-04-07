#pragma once

// The central queue used by auxilliary threads to pass events to the main thread.
typedef struct EventQueue EventQueue;

// Allocate a new [EventQueue].
// Only one [EventQueue] should be used per MPL instance.
EventQueue *EventQueue_new();
// Free an EventQueue allocated using [EventQueue_new]
void EventQueue_free(EventQueue *eq);
