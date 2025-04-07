#pragma once
#include "error.h"

// MPL interface events that can be received on the main thread.
// Some of these events will have associated body data.
#define MPL_EVENT_ENUM(VARIANT) \
	VARIANT(mpl_PLAY) \
	VARIANT(mpl_PAUSE) \
	VARIANT(mpl_STOP) \
	VARIANT(mpl_TIMECODE) \ 

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
	void *body;
} Event;

// TODO: implement timecode body
