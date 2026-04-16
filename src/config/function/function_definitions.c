#include "function_definitions.h"
#include "audio/seek.h"
#include "config/function/dictionary.h"
#include "config/function/function.h"
#include "error.h"
#include "state.h"
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


/* #region Argument parsing */

enum ConfigFn_ERR argparse_noArgs(void **args, StrtoknState *parse_state) {
	*args = NULL;
	if (strtokn(parse_state, ")") == -1) {
		return ConfigFn_SYNTAX_ERR;
	} else if (parse_state->tok_len > 0) {
		return ConfigFn_INVALID_ARG;
	}
	return ConfigFn_OK;
}
enum ConfigFn_ERR argparse_seekArgs(struct seekArgs **args, StrtoknState *parse_state) {
	// 1 arg: milliseconds (int32_t)
	if (strtokn(parse_state, ")") == -1) {
		return ConfigFn_SYNTAX_ERR;
	}
	*args = malloc(sizeof(struct seekArgs));
	CHECK_ALLOC(args, ConfigFn_BAD_ALLOC);
	char *arg_str = strndup(&parse_state->s[parse_state->offset], parse_state->tok_len);
	if (!arg_str) {
		free(*args);
		return ConfigFn_BAD_ALLOC;
	}
	if (sscanf(arg_str, "%d", &(*args)->ms) != 1) {
		free(arg_str);
		free(*args);
		return ConfigFn_INVALID_ARG;
	}
	free(arg_str);
	return ConfigFn_OK;
}

/* #endregion */

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
