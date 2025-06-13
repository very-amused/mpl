#pragma once
#include "config/config.h"
#include "queue/queue.h"
#include "ui/event_queue.h"

// State passed to MPL's bindable functions
struct configState {
	Queue *queue; // used to control playback
	EventQueue *evt_queue; // used to send exit messages
};

// State passed ONLY to direct-eval functions
// These functions manipulate the state of mpl.conf (rather than controlling MPL)
struct evalState {
	mplConfig *config;
};



// Initialize configuration state to be passed to bindable functions defined in
// config/functions.h
//
// NOTE: configState holds non-owning references and is only used on the main thread,
// so there is no configState_deinit()
void configState_init(Queue *track_queue, EventQueue *evt_queue);
// Initialize direct-eval state to passed to direct-eval functions defined in
// config/functions.h
void evalState_init(mplConfig *config);
