#pragma once

// A thread that handles a nonblocking buffer loop
typedef struct BufferThread BufferThread;

// Allocate a new BufferThread for use
BufferThread *BufferThread_new();
// Join, deinitialize, and free a BufferThread
void BufferThread_free(BufferThread *bt);
