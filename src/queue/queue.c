#include "queue.h"
#include "audio/buffer.h"
#include "audio/out/backend.h"
#include "audio/out/backends.h"
#include "audio/pcm.h"
#include "audio/seek.h"
#include "audio/track.h"
#include "buffer_thread.h"
#include "state.h"
#include "track.h"
#include "error.h"
#include "util/log.h"

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

	// Initialize state enums
	q->playback_state = Queue_STOPPED;

	// Initialize buffer thread
	q->buffer_thread = BufferThread_new();

	return 0;
}
// Deinitialize a queue and disconnect audio output.
void Queue_deinit(Queue *q) {
	/* NOTE: We have to stop the buffer thread before disconnecting audio,
	otherwise we have a deadlock b/c AudioTrack_buffer_packet() will hang forever
	due to the AudioBuffer's read semaphore being dead after audio is disconnected */
	BufferThread_free(q->buffer_thread);
	if (q->backend) {
		Queue_disconnect_audio(q);
	}
	Queue_clear(q);
}


const Track *Queue_cur_track(const Queue *q) {
	return q->cur != q->head ? q->cur->track : NULL;
}
const Track *Queue_next_track(const Queue *q) {
	return q->cur->next != q->head ? q->cur->next->track : NULL;
}

int Queue_append(Queue *q, Track *t) {
	// Wrap the track in a QueueNode
	QueueNode *node = malloc(sizeof(QueueNode));
	node->track = t; // This takes ownership of *t

	// Position node between tail and head
	node->prev = q->tail;
	node->next = q->head;
	// Link node in its position
	node->prev->next = node;
	node->next->prev = node;
	q->tail = node;

	if (q->cur == q->head) {
		return Queue_select(q, node);
	}
	return 0;
}
int Queue_prepend(Queue *q, Track *t) {
	QueueNode *node = malloc(sizeof(QueueNode));
	node->track = t;

	// Position node between head and first track
	node->prev = q->head;
	node->next = q->head->next;
	// Link node in its position
	node->prev->next = node;
	node->next->prev = node;
	q->head->next = node;

	if (q->cur == q->head) {
		return Queue_select(q, node);
	}
	return 0;
}
int Queue_insert(Queue *q, Track *t, bool before) {
	QueueNode *node = malloc(sizeof(QueueNode));
	node->track = t;

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

	if (q->cur == q->head) {
		return Queue_select(q, node);
	}
	return 0;
}

int Queue_clear(Queue *q) {
	QueueNode *node = q->head->next;
	while (node != q->head) {
		LOG(Verbosity_VERBOSE, "Freeing track %s\n", node->track->url);
		QueueNode *const next = node->next;
		Track_free(node->track);
		free(node);

		node = next;
	}
	free(q->head);

	return 0;
}

int Queue_select(Queue *q, QueueNode *node) {

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
		// Load track metadata
		at_err = AudioTrack_get_metadata(node->track->audio, &node->track->meta);
		if (at_err != AudioTrack_OK) {
			fprintf(stderr, "Failed to read metadata for track %s: %s\n", node->track->url, AudioTrack_ERR_name(at_err));
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
		return status;
	}

	return 0;
}

int Queue_play(Queue *q, bool pause) {
	int status = AudioBackend_play(q->backend, pause);
	if (status != 0) {
		return status;
	}
	q->playback_state = pause ? Queue_PAUSED : Queue_PLAYING;
	if (pause) {
		BufferThread_play(q->buffer_thread, false);
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

int Queue_seek(Queue *q, int32_t offset_ms, enum AudioSeek from) {
	AudioTrack *cur_audio = q->cur != q->head ? q->cur->track->audio : NULL;
	if (!cur_audio) {
		// TODO: err code
		return 1;
	}

	// Convert offset into bytes
	const int32_t offset_abs = offset_ms < 0 ? -offset_ms : offset_ms;
	const int32_t offset_bytes = AudioPCM_buffer_size(&cur_audio->pcm, offset_abs) * (offset_ms < 0 ? -1 : 1);

	// Pause buffering so we can adjust the buffer's write index
	BufferThread_play(q->buffer_thread, false);

	// Lock AudioBackend so we can adjust the buffer's read index
	AudioBackend_lock(q->backend);
	{
		if (from != AudioSeek_Relative) {
			fprintf(stderr, "Warning: only AudioSeek_Relative is currently supported\n");
			AudioBackend_unlock(q->backend);
			BufferThread_play(q->buffer_thread, true);
			return 1;
		}

		// Try to seek in-buffer
		if (AudioBuffer_seek(cur_audio->buffer, offset_bytes, from) != 0) {
			fprintf(stderr, "In-buffer seek failed, bailing out\n");
			AudioBackend_unlock(q->backend);
			BufferThread_play(q->buffer_thread, true);
			return 1;
		}
		AudioBackend_seek(q->backend);
	}
	AudioBackend_unlock(q->backend);
	BufferThread_play(q->buffer_thread, true);

	return 0;
}

// Get playback state from the queue and its AudioBackend
enum Queue_PLAYBACK_STATE Queue_get_playback_state(const Queue *q) {
	return q->playback_state;
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
