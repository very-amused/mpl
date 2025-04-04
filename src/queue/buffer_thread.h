#pragma once
#include "audio/track.h"

// A thread that handles a nonblocking buffer loop
typedef struct BufferThread BufferThread;

// Allocate a new BufferThread for use
BufferThread *BufferThread_new();
// Join, deinitialize, and free a BufferThread
void BufferThread_free(BufferThread *thr);

// Start a BufferThread to buffer track *t in the background.
// Returns 0 on success, nonzero on error
int BufferThread_start(BufferThread *thr, AudioTrack *track, AudioTrack *next_track);
