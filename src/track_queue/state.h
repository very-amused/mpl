#pragma once
#include "error.h"

#define QUEUE_PLAYBACK_STATE_ENUM(VARIANT) \
	VARIANT(Queue_PLAYING) \
	VARIANT(Queue_PAUSED) \
	VARIANT(Queue_STOPPED)

enum Queue_PLAYBACK_STATE {
	QUEUE_PLAYBACK_STATE_ENUM(ENUM_VAL)
};
static inline const char *Queue_PLAYBACK_STATE_name(enum Queue_PLAYBACK_STATE state) {
	switch (state) {
		QUEUE_PLAYBACK_STATE_ENUM(ENUM_KEY)
	}
	return NULL;
}

#undef QUEUE_PLAYBACK_STATE_ENUM


