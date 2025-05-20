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
int BufferThread_start(BufferThread *thr, AudioTrack *track, AudioTrack *next_track);

// Play/pause buffering
void BufferThread_play(BufferThread *thr, bool play);

/* Locking a BufferThread is very similar to pausing it,
except the lock/unlock methods use a semaphore to enable recursive locking.
This is necessary for things like the ability to seek (which locks and unlocks the BufferThread)
while paused (which also locks and unlocks the BufferThread).
Eventually, we might deprecate BufferThread_play() in favor of the lock/unlock methods.*/

// Lock a BufferThread, pausing its operation until unlocked using BufferThread_unlock().
// Recursive locking is enabled through an internal semaphore.
void BufferThread_lock(BufferThread *thr);
// Unlock a BufferThread, resuming its operation.
// Recursive locking is enabled through an internal semaphore.
// NOTE: BufferThread_unlock() will return EAGAIN when the thread hasn't actually been unlocked.
//
// Returns 0 on success.
int BufferThread_unlock(BufferThread *thr);
