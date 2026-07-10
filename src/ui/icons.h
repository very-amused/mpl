#pragma once
#include "track_queue/state.h"
#include "util/log.h"

#include <stdlib.h>

// TODO: add nerdfont icons behind a feature flag

#ifdef NERDFONT
static const char Icon_PLAY[] = "󰐊";
static const char Icon_PAUSE[] = "󰏤";
static const char Icon_STOP[] = "󰓛";
#else
static const char Icon_PLAY[] = "(Playing)";
static const char Icon_PAUSE[] = "(Paused)";
static const char Icon_STOP[] = "(Stopped)";
#endif

static inline const char *get_playback_icon(enum Queue_PLAYBACK_STATE playback_state) {
	switch (playback_state) {
		case Queue_PLAYING:
			return Icon_PLAY;
		case Queue_PAUSED:
			return Icon_PAUSE;
		case Queue_STOPPED:
			return Icon_STOP;
	}
	return DEFAULT_ERR_NAME;
}
