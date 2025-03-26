#include "queue.h"
#include "audio/out/backend.h"
#include "audio/track.h"
#include "lock.h"
#include "track.h"

#include <pthread.h>

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
