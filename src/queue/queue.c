#include "queue.h"
#include "audio/out/backend.h"
#include "audio/out/backends.h"
#include "audio/pcm.h"
#include "audio/seek.h"
#include "audio/track.h"
#include "buffer_thread.h"
#include "lock.h"
#include "state.h"
#include "track.h"
#include "error.h"

#include <pthread.h>
#include <semaphore.h>

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

	// Initialize state enums
	q->playback_state = Queue_STOPPED;

	// Initialize buffer thread
	q->buffer_thread = BufferThread_new();

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


const Track *Queue_cur_track(const Queue *q) {
	return q->cur != q->head ? q->cur->track : NULL;
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

	if (QueueLock_unlock(q->lock) != 0) {
		return 1;
	}

	if (q->cur == q->head) {
		return Queue_select(q, node);
	}
	return 0;
}
int Queue_prepend(Queue *q, Track *t) {
	QueueNode *node = malloc(sizeof(QueueNode));
	node->track = t;

	if (QueueLock_lock(q->lock) != 0) {
		return 1;
	}

	// Position node between head and first track
	node->prev = q->head;
	node->next = q->head->next;
	// Link node in its position
	node->prev->next = node;
	node->next->prev = node;
	q->head->next = node;

	if (QueueLock_unlock(q->lock) != 0) {
		return 1;
	}

	if (q->cur == q->head) {
		return Queue_select(q, node);
	}
	return 0;
}
int Queue_insert(Queue *q, Track *t, bool before) {
	QueueNode *node = malloc(sizeof(QueueNode));
	node->track = t;

	if (QueueLock_lock(q->lock) != 0) {
		return 1;
	}

	if (before) {
		// Position node between cur->prev and cur
		node->prev = q->cur->prev;
		node->next = q->cur;
		// Link node in its position
		node->prev->next = node;
		node->next->prev = node;
	} else {
		// Position node between cur and cur->next
		node->prev = q->cur;
		node->next = q->cur->next;
		// Link node in its position
		node->prev->next = node;
		node->next->prev = node;
	}

	if (QueueLock_unlock(q->lock) != 0) {
		return 1;
	}

	if (q->cur == q->head) {
		return Queue_select(q, node);
	}
	return 0;
}

int Queue_clear(Queue *q) {
	// TODO: AudioBackend_stop(q->backend);
	if (QueueLock_lock(q->lock) != 0) {
		return 1;
	}

	QueueNode *node = q->head->next;
	while (node != q->head) {
		QueueNode *const next = node->next;
		Track_free(node->track);
		free(node);

		node = next;
	}
	free(q->head);

	return QueueLock_unlock(q->lock);
}

int Queue_select(Queue *q, QueueNode *node) {
	if (QueueLock_lock(q->lock) != 0) {
		return 1;
	}

	q->cur = node;
	// FIXME: if the queue is playing we want to enqueue and then skip with the backend. otherwise we do what's below

	// Initialize track audio if needed
	if (!node->track->audio) {
		node->track->audio = malloc(sizeof(AudioTrack));
		enum AudioTrack_ERR at_err = AudioTrack_init(node->track->audio, node->track->url);
		if (at_err != AudioTrack_OK) {
			fprintf(stderr, "Failed to initialize AudioTrack for track %s: %s\n", node->track->url, AudioTrack_ERR_name(at_err));
			free(node->track->audio);
			node->track->audio = NULL;
			return 1;
		}
		// Buffer 3 seconds of audio data
		at_err = AudioTrack_buffer_ms(node->track->audio, AudioSeek_Relative, 3000);
		if (at_err != AudioTrack_OK) {
			fprintf(stderr, "Failed to initialize AudioTrack for track %s: %s\n", node->track->url, AudioTrack_ERR_name(at_err));
			AudioTrack_deinit(node->track->audio);
			free(node->track->audio);
			node->track->audio = NULL;
			return 1;
		}
	}
	// Prepare track to start playback on a new audio stream
	int status = AudioBackend_prepare(q->backend, node->track->audio);
	if (status != 0) {
		QueueLock_unlock(q->lock);
		return status;
	}

	return QueueLock_unlock(q->lock);
}

int Queue_play(Queue *q, bool pause) {
	int status = AudioBackend_play(q->backend, pause);
	if (status != 0) {
		return status;
	}
	q->playback_state = pause ? Queue_PAUSED : Queue_PLAYING;
	if (pause) {
		BufferThread_play(q->buffer_thread, pause);
		return 0;
	}

	// Start a nonblocking buffer loop
	AudioTrack *cur_audio = q->cur != q->head ? q->cur->track->audio : NULL;
	AudioTrack *next_audio = q->cur->next != q->head ? q->cur->next->track->audio : NULL;
	if (!cur_audio) {
		// TODO: implement error code
		return 1;
	}
	return BufferThread_start(q->buffer_thread, cur_audio, next_audio);
}

float Queue_cur_timestamp(const Queue *q) {
	if (q->cur == q->head) {
		return 0;
	}
	const AudioTrack *audio = q->cur->track->audio;
	return AudioPCM_seconds(&audio->pcm, audio->buffer->n_read);
}

// Get playback state from the queue and its AudioBackend
enum Queue_PLAYBACK_STATE Queue_get_playback_state(const Queue *q) {
	// TODO
	return Queue_PLAYING;
}

int Queue_connect_audio(Queue *q, AudioBackend *ab, const EventQueue *eq) {
	// Set q->backend to a defined AudioBackend
	if (ab) {
		q->backend = ab;
	} else {
		q->backend = &AB_PulseAudio;
	}

	// Initialize the backend
	return AudioBackend_init(q->backend, eq);
}

// Disconnect the queue from the system's audio backend. Frees q->backend
void Queue_disconnect_audio(Queue *q) {
	AudioBackend_deinit(q->backend);
}
