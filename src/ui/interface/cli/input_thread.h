#pragma once

// A thread that handles passing input to an EventQueue without blocking
#include "ui/event_queue.h"
typedef struct InputThread InputThread;

// InputMode tells the UI how to collect input data
enum InputMode {
	InputMode_KEY, // Get one key of input at a time without buffering
	InputMode_TEXT // Get buffered text input
};

// Allocate a new InputThread for use and immediately start receiving user input
InputThread *InputThread_new(EventQueue *eq);
// Join, deinitialize, and free an InputThread
void InputThread_free(InputThread *thr);

void InputThread_set_mode(InputThread *thr, enum InputMode mode);
