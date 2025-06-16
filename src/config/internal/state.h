#pragma once
#include "config/config.h"
#include "queue/queue.h"
#include "ui/event_queue.h"

// State passed to MPL's bindable functions
struct configState {
	Queue *queue; // used to control playback
	EventQueue *evt_queue; // used to send exit messages
};

// State passed only to macros
// Macros are functions that manipulate the state of mpl.conf (rather than controlling MPL)
struct macroState {
	Config *config;
};



// Initialize configuration state to be passed to bindable functions defined in
// config/functions.h
//
// NOTE: configState holds non-owning references and is only used on the main thread,
// so there is no configState_deinit()
void configState_init(Queue *track_queue, EventQueue *evt_queue);
// Initialize configuration state to passed to macro functions defined in
// config/functions.h
void macroState_init(Config *config);
