#pragma once

#include <pthread.h>

// Dummy asynchronous loop that consumes audio frames on a clock on a separate thread.
// Like all async loops, most API methods require holding a lock over the loop
typedef struct DummyLoop DummyLoop;

// Allocate and initialize a DummyLoop for use
// NOTE: the loop is inactive until started with [DummyLoop_start]
DummyLoop *DummyLoop_new();
// Deinitialize and free a DummyLoop
// NOTE: the loop must be stopped with DummyLoop_stop beforehand
void DummyLoop_free(DummyLoop *loop);

// Start a DummyLoop, making it ready for all methods to be called (i.e DummyLoop_play)
// Returns 0 on success, nonzero on error
int DummyLoop_start(DummyLoop *loop);
// Stop a DummyLoop, causing its clock to stop running
void DummyLoop_stop(DummyLoop *loop);
