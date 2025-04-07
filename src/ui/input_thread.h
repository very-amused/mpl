#pragma once

// A thread that handles passing input to an EventQueue without blocking
typedef struct InputThread InputThread;

// Allocate a new InputThread for use
InputThread *InputThread_new();
// Join, deinitialize, and free an InputThread
void InputThread_free(InputThread *thr);

// Start an input thread to receive input in the background.
void InputThread_start(InputThread *thr);
