#pragma once
#include "error.h"
#include "track_queue/state.h"
#include "ui/event.h"

#include <stdint.h>

// Events the TermIOThread must wake and respond to
#define TERMIO_EVENT_ENUM(VARIANT) \
	VARIANT(TermIO_SHUTDOWN) /* Thread shutdown */ \
	VARIANT(TermIO_CHANGE_MODE) /* Change IO mode (key / shell) */ \
	VARIANT(TermIO_TIMECODE) /* Timecode update received */ \
	VARIANT(TermIO_PLAYBACK_STATE) /* Play/pause/stop state change from TrackQueue */

enum TermIO_Event_t {
	TERMIO_EVENT_ENUM(ENUM_VAL)
};

static const inline char *TermIO_Event_t(enum TermIO_Event_t evt) {
	switch (evt) {
		TERMIO_EVENT_ENUM(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef TERMIO_EVENT_ENUM

// A MPL event message send from the main thread to the TermIOThread
typedef struct TermIO_Event {
	enum TermIO_Event_t event_type;
	// We have two inline bodies so we can send timecodes with current track duration
	uint64_t body_inline;
	uint64_t body_inline2;
} TermIO_Event;

// Timecodes sent with TermIO_TIMECODE events:
// body_inline is the position timecode,
// body_inline2 is the duration timecode
typedef EventBody_Timecode TermIO_EventBody_Timecode;

// Playback state sent with TermIO_PLAYBACK_STATE events
typedef enum Queue_PLAYBACK_STATE TermIO_EventBody_PlaybackState;
