#pragma once
#include "audio/out/backend.h"
#include "lock.h"
#include "track.h"
#include <bits/pthreadtypes.h>

typedef struct QueueNode QueueNode;
typedef struct QueueNode {
	Track *track;
	QueueNode *prev;
	QueueNode *next;
} QueueNode;

// A queue performing non-blocking track management
typedef struct Queue {
	QueueNode *cur; // Currently playing track
	QueueNode *head; // Top of the queue (sentinel node). head->next is the first actual track (iff head->next != head)
	QueueNode *tail; // Bottom of the queue (last playable track). tail->next == head

	AudioBackend *backend;

	QueueLock *lock;
} Queue;

// Initialize an empty queue.
int Queue_init(Queue *q);
// Deinitialize a queue and disconnect audio output.
void Queue_deinit(Queue *q);

// Clear all tracks in a queue
// NOTE: locks the queue
void Queue_clear(Queue *q);

// Connect the queue to the system's audio output.
// If ab == NULL, a 'best' audio backend for the system will be automatically determined. This strategy is recommended
int Queue_connect_audio(Queue *q, AudioBackend *ab);
// Disconnect the queue from the system's audio output.
int Queue_disconnect_audio(Queue *q);
