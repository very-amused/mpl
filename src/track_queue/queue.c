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
#include "ui/event.h"
#include "ui/event_queue.h"
#include "util/log.h"

#include <pthread.h>
#include <semaphore.h>
#include <sys/param.h>

struct TrackQueueNode {
	Track *track;
	TrackQueueNode *prev;
	TrackQueueNode *next;
};

// Initialize an empty queue
int TrackQueue_init(TrackQueue *q, const Settings *settings, EventQueue *eq) {
	q->head = malloc(sizeof(TrackQueueNode));
	CHECK_ALLOC(q->head, 1);
	q->head->prev = q->head->next = q->head;
	q->cur = q->tail = q->head;

	// Don't automatically connect to any backend, in case the user wants to choose a specific backend
	q->backend = NULL;
	// We do automatically connect this however
	q->evt_sq = EventQueue_connect(eq, 5);

	// Initialize state enums
	q->playback_state = Queue_STOPPED;

	// Initialize buffer thread
	q->buffer_thread = BufferThread_new();
	q->prebuffer_thread = BufferThread_new();

	q->settings = settings;

	return 0;
}
// Deinitialize a queue and disconnect audio output.
void TrackQueue_deinit(TrackQueue *q) {
	/* NOTE: We have to stop the buffer thread before disconnecting audio,
	otherwise we have a deadlock b/c AudioTrack_buffer_packet() will hang forever
	due to the AudioBuffer's read semaphore being dead after audio is disconnected */
	BufferThread_free(q->buffer_thread);
	BufferThread_free(q->prebuffer_thread);
	if (q->backend) {
		TrackQueue_disconnect_audio(q);
	}
	TrackQueue_clear(q);
}

enum AudioBackend_ERR TrackQueue_connect_audio(TrackQueue *q, const Settings *settings, EventQueue *eq) {
	// Set q->backend to a defined AudioBackend
	q->backend = AB_Configured(settings);

	// Initialize the backend
	return AudioBackend_init(q->backend, eq, q->settings);
}
// Disconnect the queue from the system's audio backend. Frees q->backend
void TrackQueue_disconnect_audio(TrackQueue *q) {
	AudioBackend_deinit(q->backend);
}


const Track *TrackQueue_cur_track(const TrackQueue *q) {
	return q->cur != q->head ? q->cur->track : NULL;
}
const Track *TrackQueue_next_track(const TrackQueue *q) {
	return q->cur->next != q->head ? q->cur->next->track : NULL;
}

int TrackQueue_append(TrackQueue *q, Track *t) {
	// Wrap the track in a QueueNode
	TrackQueueNode *node = malloc(sizeof(TrackQueueNode));
	CHECK_ALLOC(node, 1);
	node->track = t; // This takes ownership of *t

	// Position node between tail and head
	node->prev = q->tail;
	node->next = q->head;
	// Link node in its position
	node->prev->next = node;
	node->next->prev = node;
	q->tail = node;

	if (q->cur == q->head) {
		return TrackQueue_select(q, node);
	}
	return 0;
}
int TrackQueue_prepend(TrackQueue *q, Track *t) {
	TrackQueueNode *node = malloc(sizeof(TrackQueueNode));
	CHECK_ALLOC(node, 1);
	node->track = t;

	// Position node between head and first track
	node->prev = q->head;
	node->next = q->head->next;
	// Link node in its position
	node->prev->next = node;
	node->next->prev = node;
	q->head->next = node;

	if (q->cur == q->head) {
		return TrackQueue_select(q, node);
	}
	return 0;
}
int TrackQueue_insert(TrackQueue *q, Track *t, bool before) {
	TrackQueueNode *node = malloc(sizeof(TrackQueueNode));
	CHECK_ALLOC(node, 1);
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
		return TrackQueue_select(q, node);
	}
	return 0;
}

int TrackQueue_clear(TrackQueue *q) {
	TrackQueueNode *node = q->head->next;
	while (node != q->head) {
		LOG(Verbosity_VERBOSE, "Freeing track %s\n", node->track->url);
		TrackQueueNode *const next = node->next;
		Track_free(node->track);
		free(node);

		node = next;
	}
	free(q->head);

	return 0;
}

int TrackQueue_select(TrackQueue *q, TrackQueueNode *node) {
	// Set the current track in the queue
	TrackQueueNode *old = q->cur;
	q->cur = node;
	if (old != q->head) {
		// Free buffers associated with the old track
		AudioTrack_deinit_buffers(&old->track->audio);
	}
	
	// Buffer track audio on the main BufferThread
	if (!node->track->audio.buffer) {
		enum AudioTrack_ERR err = AudioTrack_init_buffers(&node->track->audio, q->settings);
		if (err != AudioTrack_OK) {
			LOG(Verbosity_NORMAL, "Failed to initialize AudioTrack buffers for track %s: %s\n", node->track->url, AudioTrack_ERR_name(err));
			return 1;
		}
	}
	// TODO: stop prebuffering if that's happening
	BufferThread_start(q->buffer_thread, &node->track->audio);

	// Prepare track to start playback on a new audio stream
	int status = AudioBackend_prepare(q->backend, &node->track->audio);
	if (status != 0) {
		return status;
	}

	return 0;
}

int TrackQueue_preselect(TrackQueue *q, TrackQueueNode *node) {
	// Handle prebuffering on our end
	

	// Handle prebuffering on the AudioBackend's end (create a stream)

	return 1;
}

// FIXME: we can do a much better job of synchonizing state between the AudioBackend and BufferThread
// I'm almost convinced the Queue struct needs a ground-up rewrite, but we'll see
int TrackQueue_play(TrackQueue *q, bool pause) {
	int status = AudioBackend_play(q->backend, pause);
	if (status != 0) {
		return status;
	}

	if (pause && q->playback_state == Queue_PLAYING) {
		q->playback_state = Queue_PAUSED;
		BufferThread_lock(q->buffer_thread);
		return 0;
	} else if (q->playback_state == Queue_PAUSED) {
		status = BufferThread_unlock(q->buffer_thread);
		if (status == 0) {
			q->playback_state = Queue_PLAYING;
		}
		return status;
	}
	q->playback_state = pause ? Queue_PAUSED : Queue_PLAYING;

	// Start a nonblocking buffer loop
	AudioTrack *cur_audio = q->cur != q->head ? &q->cur->track->audio : NULL;
	if (!cur_audio) {
		// TODO: implement error code
		// FIXME: holy hell we never got around to this. Queue_ERR needed!
		return 1;
	}
	return BufferThread_start(q->buffer_thread, cur_audio);
}

// The inner logic of Queue_seek that holds a lock on an AudioBuffer (by pausing its BufferThread) and AudioBackend
// NOTE: This function does NOT release these locks. That is left up to the caller.
//
// This is its own function so this logic can be called from both [Queue_seek] and [Queue_seek_snap]
static int Queue_seek_inner(TrackQueue *q, int32_t offset, enum AudioSeek from, AudioTrack *cur_audio) {
	if (from != AudioSeek_Relative) {
		LOG(Verbosity_NORMAL, "Warning: only AudioSeek_Relative is currently supported\n");
		return 1;
	}

	// Try to seek in-buffer
	{
		enum AudioBuffer_ERR ab_err;
		if (ab_err = AudioBuffer_seek(cur_audio->buffer, offset, from), ab_err != AudioBuffer_OK) {
			LOG(Verbosity_NORMAL, "In-buffer seek failed (%s), bailing out\n", AudioBuffer_ERR_name(ab_err));
			return 1;
		}
	}
	// Apply seek with audio backend
	AudioBackend_seek(q->backend);

	// Send updated timecode to main thread
	// (this is needed so seeks when paused are visible)
	Event evt = {
		.event_type = mpl_TIMECODE,
		.body_size = sizeof(EventBody_Timecode),
		.body_inline = cur_audio->buffer->n_read / cur_audio->buffer->frame_size
	};
	EventSubQueue_send(q->evt_sq, &evt, false);

	return 0;
}

int TrackQueue_seek(TrackQueue *q, int32_t offset_ms, enum AudioSeek from) {
	AudioTrack *cur_audio = q->cur != q->head ? &q->cur->track->audio : NULL;
	if (!cur_audio) {
		// TODO: err code
		return 1;
	}

	// Convert offset into bytes
	const int32_t offset_ms_abs = offset_ms < 0 ? -offset_ms : offset_ms;
	const int32_t offset = AudioPCM_buffer_size(&cur_audio->buf_pcm, offset_ms_abs) * (offset_ms < 0 ? -1 : 1);

	// Pause buffering so we can adjust the buffer's write index
	BufferThread_lock(q->buffer_thread);
	// Lock AudioBackend so we can adjust the buffer's read index
	AudioBackend_lock(q->backend);
	int status;
	{
		status = Queue_seek_inner(q, offset, from, cur_audio);
	}
	AudioBackend_unlock(q->backend);
	BufferThread_unlock(q->buffer_thread);

	return status;
}

// WIP
int TrackQueue_seek_snap(TrackQueue *q, int32_t offset_ms) {
	
	AudioTrack *cur_audio = q->cur != q->head ? &q->cur->track->audio : NULL;
	if (!cur_audio) {
		return 1;
	}

	const int32_t offset_ms_abs = offset_ms < 0 ? -offset_ms : offset_ms;
	// Offset scalar in bytes
	const int32_t offset_scalar = AudioPCM_buffer_size(&cur_audio->buf_pcm, offset_ms_abs);

	// Lock everything else that touches the track's AudioBuffer
	BufferThread_lock(q->buffer_thread);
	AudioBackend_lock(q->backend);

	// Convert offset into bytes, this will be an even multiple of frame_size since we use AudioPCM_buffer_size
	int32_t offset = offset_scalar * (offset_ms < 0 ? -1 : 1);

	// Compute seek snap alignment so we get a projected n_read value that's an even multiple of offset_scalar
	const ssize_t n_read = cur_audio->buffer->n_read,
				frame_size = cur_audio->buffer->frame_size,
				sample_rate = cur_audio->buf_pcm.sample_rate;
	const ssize_t fps = frame_size * sample_rate; // frames per second
	const ssize_t time_ms = (1000 * n_read) / fps;

	// TODO: comment what's going on here
	const ssize_t ms_from_last = time_ms % offset_ms_abs;
	static const size_t SNAP_WINDOW_MS = 1000;
	if (offset < 0) {
		offset += offset_scalar - (n_read % offset_scalar);
		if (ms_from_last <= SNAP_WINDOW_MS && n_read > 0) {
			offset -= offset_scalar;
		}
	} else {
		offset -= n_read % offset_scalar;
	}

	int status = Queue_seek_inner(q, offset, AudioSeek_Relative, cur_audio);

	AudioBackend_unlock(q->backend);
	BufferThread_unlock(q->buffer_thread);

	return status;
}

// Get playback state from the queue and its AudioBackend
enum Queue_PLAYBACK_STATE Queue_get_playback_state(const TrackQueue *q) {
	return q->playback_state;
}
