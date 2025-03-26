#include "queue.h"
#include "audio/out/backend.h"
#include "audio/track.h"
#include "lock.h"
#include "track.h"

#include <pthread.h>

struct QueueNode {
	Track *track;
	QueueNode *prev;
	QueueNode *next;
};

// Initialize an empty queue
int Queue_init(Queue *q) {
	q->head = malloc(sizeof(QueueNode));
	q->head->prev = q->head->next = q->head;
	q->cur = q->tail = q->head;

	// Don't automatically connect to any backend, in case the user wants to choose a specific backend
	q->backend = NULL;

	// Initialize queue locking
	QueueLock_init(&q->lock);

	return 0;
}
// Deinitialize a queue and disconnect audio output.
void Queue_deinit(Queue *q) {
	if (q->backend) {
		Queue_disconnect_audio(q);
	}
	Queue_clear(q);

	// Free the queue's lock
	free(q->lock);
}

int Queue_append(Queue *q, Track *t) {
	// Wrap the track in a QueueNode
	QueueNode *node = malloc(sizeof(QueueNode));
	node->track = t; // This takes ownership of *t

	if (QueueLock_lock(q->lock) != 0) {
		return 1;
	}

	// Position node between tail and head
	node->prev = q->tail;
	node->next = q->head;
	// Link node in its position
	node->prev->next = node;
	node->next->prev = node;
	q->tail = node;

	return QueueLock_unlock(q->lock);
}
int Queue_prepend(Queue *q, Track *t) {
	QueueNode *node = malloc(sizeof(QueueNode));
	node->track = t;

	if (QueueLock_lock(q->lock) != 0) {
		return 1;
	}

	// Position node between head and first track
	QueueNode *const first = q->head->next;
	node->prev = q->head;
	node->next = q->head->next;
	// Link node in its position
	node->prev->next = node;
	node->next->prev = node;
	q->head->next = node;

	return QueueLock_unlock(q->lock);
}
int Queue_insert(Queue *q, Track *t, bool before) {
	QueueNode *node = malloc(sizeof(QueueNode));
	node->track = t;

	if (QueueLock_lock(q->lock) != 0) {
		return 1;
	}

	if (before) {
		// Position node between cur->prev and cur
		QueueNode *const cur = q->cur;
		node->prev = cur->prev;
		node->next = cur;
		// Link node in its position
		node->prev->next = node;
		node->next->prev = node;
	} else {
		// Position node between cur and cur->next
		QueueNode *const cur = q->cur;
		node->prev = cur;
		node->next = cur->next;
		// Link node in its position
		node->prev->next = node;
		node->next->prev = node;
	}

	return QueueLock_unlock(q->lock);
}

void Queue_clear(Queue *q) {
	// TODO: AudioBackend_stop(q->backend);
	QueueLock_lock(q->lock);

	QueueNode *node = q->head->next;
	while (node != q->head) {
		QueueNode *const next = node->next;
		Track_deinit(node->track);
		free(node->track);
		free(node);

		node = next;
	}

	QueueLock_unlock(q->lock);
}

// Connect the queue to the system's audio backend. Allocates q->backend
int Queue_connect_audio(Queue *q, AudioBackend *ab);
// Disconnect the queue from the system's audio backend. Frees q->backend
int Queue_disconnect_audio(Queue *q);
