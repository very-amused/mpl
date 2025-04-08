#pragma once
#include "error.h"
#include <stdint.h>
#include <stdbool.h>

// MPL interface events that can be received on the main thread.
// Some of these events will have associated body data.
#define MPL_EVENT_ENUM(VARIANT) \
	VARIANT(mpl_KEYPRESS) \
	VARIANT(mpl_TIMECODE)

enum MPL_EVENT {
	MPL_EVENT_ENUM(ENUM_VAL)
};

static inline const char *MPL_EVENT_name(enum MPL_EVENT evt) {
	switch (evt) {
		MPL_EVENT_ENUM(ENUM_KEY)
	}
	return NULL;
}

#undef MPL_EVENT_ENUM

// An MPL event message sent on the EventQueue
typedef struct Event {
	enum MPL_EVENT event_type;
	size_t body_size;
	void *body; // pointer body used to pass types > 8 bytes
	uint64_t body_inline; // inline body used to pass types <= 8 bytes
} Event;

typedef char EventBody_Keypress;

// The number of audio frames that have been played in the current track.
// Divide by the track's PCM->sample_rate to get the timecode in seconds.
typedef uint64_t EventBody_Timecode;

// TODO: implement timecode body
