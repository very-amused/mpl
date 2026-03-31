#pragma once
#include "error.h"

#define TRACK_QUEUE_PLAYBACK_STATE_ENUM(VARIANT) \
	VARIANT(Queue_PLAYING) \
	VARIANT(Queue_PAUSED) \
	VARIANT(Queue_STOPPED)

enum TrackQueue_PLAYBACK_STATE {
	TRACK_QUEUE_PLAYBACK_STATE_ENUM(ENUM_VAL)
};
static inline const char *TrackQueue_PLAYBACK_STATE_name(enum TrackQueue_PLAYBACK_STATE state) {
	switch (state) {
		TRACK_QUEUE_PLAYBACK_STATE_ENUM(ENUM_KEY)
	}
	return NULL;
}

#undef TRACK_QUEUE_PLAYBACK_STATE_ENUM

