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


// Open a new subqueue that feeds events to *eq.
// Once returned, this subqueue may be used from a non-main thread.
// [subqueue_size] is the number of events the subqueue can buffer before getting overwhelmed.
// WARN: This routine MUST be called on the main thread.
EventSubQueue *EventQueue_connect(EventQueue *eq, size_t subqueue_size);

// Send an Event via this subqueue to the main EventQueue.
// Makes a copy of *evt (you don't need to heap-allocate events)
// NOTE: may block if the subqueue is full. If this isn't desired, pass allow_drop=true,
// which causes the subqueue to drop new events when it doesn't have room for them.
void EventSubQueue_send(EventSubQueue *sq, Event *evt, bool allow_drop);

// Wait to receive an event on the EventQueue
// NOTE: May allocate evt->body, caller is responsible for freeing
// Returns 0 on success, nonzero on error
int EventQueue_recv(EventQueue *eq, Event *evt);

/* #region deprecated */
// WARN: DEPRECATED in favor of EventQueue_connect
EventQueue *EventQueue_connect_legacy(const EventQueue *eq1, int oflags);
// Send *evt on the EventQueue (*evt is copied).
// Returns 0 on success, nonzero on error
// WARN: DEPRECATED in favor of EventSubQueue_send.
int EventQueue_send(EventQueue *eq, const Event *evt);
/* #endregion */
