
#pragma once
#include "config/config.h"
#include "track_queue/queue.h"
#include "ui/event_queue.h"

// State passed to MPL's non-macro functions
struct fnState {
	TrackQueue *queue; // used to control playback
	EventSubQueue *evt_sq; // used to control UI by sending events
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
void ConfigFn_fnState_init(TrackQueue *track_queue, EventQueue *evt_queue);
// Initialize configuration state to passed to macro functions defined in
// config/functions.h
void ConfigFn_macroState_init(Config *config);
