#include "function_definitions.h"
#include "audio/seek.h"
#include "config/function/dictionary.h"
#include "config/function/function.h"
#include "error.h"
#include "state.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "track_queue/queue.h"
#include "track_queue/state.h"
#include "util/strtokn.h"

/* #region Config function state */

static struct fnState state;

void ConfigFn_fnState_init(TrackQueue *track_queue, EventQueue *eq) {
	state.queue = track_queue;
	state.evt_sq = EventQueue_connect(eq, 5);
}

/* #endregion */


void play(void * _) {
	TrackQueue_play(state.queue, false);
}
void pause(void * _) {
	TrackQueue_play(state.queue, true);
}
void play_toggle(void * _) {
	const bool pause = state.queue->playback_state == Queue_PLAYING;
	TrackQueue_play(state.queue, pause);
}

void quit(void * _) {
	static const Event quit_evt = {
		.event_type = mpl_QUIT,
		.body_size = 0
	};
	EventSubQueue_send(state.evt_sq, &quit_evt, false);
}

void shell_open(void * _) {
	static const Event evt = {
		.event_type = mpl_SHELL_OPEN,
		.body_size = 0
	};
	EventSubQueue_send(state.evt_sq, &evt, false);
}
void shell_close(void * _) {
	static const Event evt = {
		.event_type = mpl_SHELL_CLOSE,
		.body_size = 0
	};
	fprintf(stderr, "yuh\n");
	EventSubQueue_send(state.evt_sq, &evt, false);
	fprintf(stderr, "yuh\n");
}

void seek(const struct seekArgs *args) {
	TrackQueue_seek(state.queue, args->ms, AudioSeek_Relative);
}
void seek_snap(const struct seekArgs *args) {
	TrackQueue_seek_snap(state.queue, args->ms);
}

void show_metadata(void * _) {
	// Get current track
	const Track *tr = TrackQueue_cur_track(state.queue);
	if (!tr) {
		return;
	}
	// Package TrackMeta as event
	Event meta_evt = {
		.event_type = mpl_TRACK_META,
		.body_size = sizeof(TrackMeta),
		.body = malloc(sizeof(EventBody_TrackMeta))
	};
	memcpy(meta_evt.body, &tr->meta, sizeof(EventBody_TrackMeta));
	// Send it
	EventSubQueue_send(state.evt_sq, &meta_evt, false);
}
