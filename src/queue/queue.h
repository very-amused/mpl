#pragma once
#include "audio/out/backend.h"
#include "lock.h"
#include "state.h"
#include "track.h"
#include "buffer_thread.h"

#include <stdbool.h>

typedef struct QueueNode QueueNode;

// A queue performing non-blocking track management
typedef struct Queue {
	QueueNode *cur; // Currently playing track
	QueueNode *head; // Top of the queue (sentinel node). head->next is the first actual track (iff head->next != head)
	QueueNode *tail; // Bottom of the queue (last playable track). tail->next == head

	AudioBackend *backend;
	BufferThread *buffer_thread;

	QueueLock *lock;

	enum Queue_PLAYBACK_STATE playback_state;
} Queue;


// Initialize an empty queue.
int Queue_init(Queue *q);
// Deinitialize a queue and disconnect audio output.
void Queue_deinit(Queue *q);

// Get the currently playing track in the queue
const Track *Queue_cur_track(const Queue *q);
// Get the next track queued after the currently playing track
const Track *Queue_next_track(const Queue *q);

// Clear all tracks in a queue
// NOTE: locks the queue
int Queue_clear(Queue *q);

// Append track *t to the end of the queue
// NOTE: takes ownership of *t
// NOTE: locks the queue
int Queue_append(Queue *q, Track *t);
// Prepend track *t  to the beginning of the queue
// NOTE: takes ownership of *t
// NOTE: locks the queue
int Queue_prepend(Queue *q, Track *t);
// Insert track *t  either ahead of or before the current track in the queue
// NOTE: takes ownership of *t
// NOTE: locks the queue
int Queue_insert(Queue *q, Track *t, bool before);

// Select a track to be q->cur. Handles playback
// NOTE: locks the queue
int Queue_select(Queue *q, QueueNode *node);
// Play or pause the currently selected track.
int	Queue_play(Queue *q, bool pause);

// Get playback state from the queue and its AudioBackend
enum Queue_PLAYBACK_STATE Queue_get_playback_state(const Queue *q);

// Connect the queue to the system's audio output.
// If ab == NULL, a 'best' audio backend for the system will be automatically determined. This strategy is recommended
int Queue_connect_audio(Queue *q, AudioBackend *ab, const EventQueue *eq);
// Disconnect the queue from the system's audio output.
void Queue_disconnect_audio(Queue *q);
