#pragma once
#include "audio/track.h"

#include <stdbool.h>

// A thread that handles a nonblocking buffer loop
typedef struct BufferThread BufferThread;

// Allocate a new BufferThread for use
BufferThread *BufferThread_new();
// Join, deinitialize, and free a BufferThread
void BufferThread_free(BufferThread *thr);

// Start a BufferThread to buffer track *t in the background.
// Returns 0 on success, nonzero on error
int BufferThread_start(BufferThread *thr, AudioTrack *track);

// Start a BufferThread to prebuffer up to prebuf_ms milliseconds of audio data in the background.
// Returns 0 on success, nonzero on error
int BufferThread_start_prebuf(BufferThread *thr, AudioTrack *track, uint32_t prebuf_ms);
// Stop a prebuffering BufferThread
// Returns 0 on success, nonzero on error
int BufferThread_stop_prebuf(BufferThread *thr);

// Get the current track this BufferThread is buffering. Automatically handles locking
const AudioTrack *BufferThread_cur_track(BufferThread *thr);

// Lock a BufferThread, pausing its operation until unlocked using BufferThread_unlock().
// Recursive locking is enabled through an internal semaphore.
void BufferThread_lock(BufferThread *thr);
// Unlock a BufferThread, resuming its operation.
// Recursive locking is enabled through an internal semaphore.
// NOTE: BufferThread_unlock() will return EAGAIN when the thread hasn't actually been unlocked.
//
// Returns 0 on success.
int BufferThread_unlock(BufferThread *thr);
