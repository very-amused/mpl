#pragma once
#include "audio/out/backend.h"
#include "audio/seek.h"
#include "config/settings.h"
#include "error.h"
#include "state.h"
#include "track.h"
#include "buffer_thread.h"
#include "ui/event_queue.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct QueueNode QueueNode;

// A queue performing non-blocking track management.
// In simple terms, this can be thought of as MPL's "backend",
// with one of the UI's serving as MPL's "frontend"
typedef struct TrackQueue {
	QueueNode *cur; // Currently playing track
	QueueNode *head; // Top of the queue (sentinel node). head->next is the first actual track (iff head->next != head)
	QueueNode *tail; // Bottom of the queue (last playable track). tail->next == head

	AudioBackend *audio_out; // audio output we send PCM to
	BufferThread *buffer_thread; // demuxing, decoding, and buffering of PCM frames
	EventQueue *evt_queue; // events sent from non-main threads to the main thread

	enum TrackQueue_PLAYBACK_STATE playback_state;

	// User settings
	const Settings *settings;
} TrackQueue;


// Initialize an empty track queue.
int TrackQueue_init(TrackQueue *q, const Settings *settings);
// Disconnect audio output and deinitialize a queue
void TrackQueue_deinit(TrackQueue *q);

// Connect the queue to the system's audio output.
// If ab == NULL, a 'best' audio backend for the system will be automatically determined from mpl.conf or defaults. This is the recommended strategy.
enum AudioBackend_ERR TrackQueue_connect_audio(TrackQueue *q, AudioBackend *ab);
// Disconnect the queue from the system's audio output.
void TrackQueue_disconnect_audio(TrackQueue *q);

// Get the currently playing track in the queue
const Track *TrackQueue_cur_track(const TrackQueue *q);
// Get the next track queued after the currently playing track
const Track *TrackQueue_next_track(const TrackQueue *q);

// Clear all tracks in a queue
int TrackQueue_clear(TrackQueue *q);

// Append track *t to the end of the queue
int TrackQueue_append(TrackQueue *q, Track *t);
// Prepend track *t  to the beginning of the queue
// NOTE: takes ownership of *t
int TrackQueue_prepend(TrackQueue *q, Track *t);
// Insert track *t  either ahead of or before the current track in the queue
// NOTE: takes ownership of *t
int TrackQueue_insert(TrackQueue *q, Track *t, bool before);

// Select a track to be q->cur. Handles playback
int TrackQueue_select(TrackQueue *q, QueueNode *node);
// Play or pause the currently selected track.
int	TrackQueue_play(TrackQueue *q, bool pause);
// Seek within the currently playing track.
// Handles necessary locks as well as buffer + decoding context updates
int TrackQueue_seek(TrackQueue *q, int32_t offset_ms, enum AudioSeek from);
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
int TrackQueue_seek_snap(TrackQueue *q, int32_t offset_ms);


// Get playback state from the queue and its AudioBackend
enum TrackQueue_PLAYBACK_STATE TrackQueue_get_playback_state(const TrackQueue *q);

