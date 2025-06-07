#pragma once
#include "queue/queue.h"
#include "ui/event_queue.h"

// State passed to MPL config-functions
struct configState {
	Queue *queue; // used to control playback
	EventQueue *evt_queue; // used to send exit messages
};

extern struct configState config_state;

// Initialize global configuration state to be passed to functions defined in
// config/functions.h
//
// NOTE: configState holds non-owning references and is only used on the main thread,
// so there is no configState_deinit()
void configState_init(Queue *track_queue, EventQueue *evt_queue);

