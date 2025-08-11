#pragma once

// TODO: Callback runner thread for DummyLoop
typedef struct DummyCallbackThread {
} DummyCallbackThread;

// Initialize a DummyCallbackThread for use; does not start.
// Intended usage is 1. initialize 2. register callbacks 3. start
void DummyCallbackThread_init(DummyCallbackThread *thr);
// Stop a DummyCallbackThread and free its resources
void DummyCallbackThread_deinit(DummyCallbackThread *thr);
