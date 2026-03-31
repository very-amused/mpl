#include "functions.h"
#include "audio/seek.h"
#include "config.h"
#include "config/internal/parse.h"
#include "config/internal/state.h"
#include "queue/queue.h"
#include "queue/state.h"
#include "ui/event.h"
#include "ui/event_queue.h"


/* Bindable functions */

static struct configState config_state;

void configState_init(TrackQueue *track_queue) {
	config_state.queue = track_queue;
	config_state.evt_sq = EventQueue_connect(track_queue->evt_queue, 5);
}

void play_toggle(void * _) {
	const bool pause = config_state.queue->playback_state == Queue_PLAYING;
	TrackQueue_play(config_state.queue, pause);
}

void quit(void * _) {
	static const Event quit_evt = {
		.event_type = mpl_QUIT,
		.body_size = 0
	};
	EventSubQueue_send(config_state.evt_sq, &quit_evt, false);
}

void seek(const struct seekArgs *args) {
	TrackQueue_seek(config_state.queue, args->ms, AudioSeek_Relative);
}

void seek_snap(const struct seekArgs *args) {
	TrackQueue_seek_snap(config_state.queue, args->ms);
}

/* Macro functions */

static struct macroState macro_state;

void macroState_init(Config *config) {
	macro_state.config = config;
}

void include_default_keybinds(void * _) {
	Config_parse_internal(macro_state.config, NULL, PARSE_KEYBINDS);
}
