#pragma once
#include "audio/out/backend.h"
#include "audio/seek.h"
#include "lock.h"
#include "state.h"
#include "track.h"
#include "buffer_thread.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct QueueNode QueueNode;

// A queue performing non-blocking track management
typedef struct Queue {
	QueueNode *cur; // Currently playing track
	QueueNode *head; // Top of the queue (sentinel node). head->next is the first actual track (iff head->next != head)
	QueueNode *tail; // Bottom of the queue (last playable track). tail->next == head

	AudioBackend *backend;
	BufferThread *buffer_thread;

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
int Queue_clear(Queue *q);

// Append track *t to the end of the queue
int Queue_append(Queue *q, Track *t);
// Prepend track *t  to the beginning of the queue
// NOTE: takes ownership of *t
int Queue_prepend(Queue *q, Track *t);
// Insert track *t  either ahead of or before the current track in the queue
// NOTE: takes ownership of *t
int Queue_insert(Queue *q, Track *t, bool before);

// Select a track to be q->cur. Handles playback
int Queue_select(Queue *q, QueueNode *node);
// Play or pause the currently selected track.
int	Queue_play(Queue *q, bool pause);
// Seek within the currently playing track.
// Handles necessary locks as well as buffer + decoding context updates
int Queue_seek(Queue *q, int32_t offset_ms, enum AudioSeek from);
// Seek within the current track, snapping to the nearest multiple of offset_ms.
// [AudioSeek_Relative] is always used as `from`.
// Examples:
// 00:01 + 5s -> 00:05
// 00:01 - 5s -> 00:00
// 00:04 + 5s -> 00:05
// 00:05 + 5s -> 00:10
// 00:06 - 5s -> 00:05
//
// This is basically Queue_seek with better UX
// WIP
int Queue_seek_snap(Queue *q, int32_t offset_ms);


// Get playback state from the queue and its AudioBackend
enum Queue_PLAYBACK_STATE Queue_get_playback_state(const Queue *q);

// Connect the queue to the system's audio output.
// If ab == NULL, a 'best' audio backend for the system will be automatically determined. This strategy is recommended
int Queue_connect_audio(Queue *q, AudioBackend *ab, const EventQueue *eq);
// Disconnect the queue from the system's audio output.
void Queue_disconnect_audio(Queue *q);
