#include "state.h"
#include "ui/event_queue.h"

/* #region Config function state */

static struct fnState state;

void ConfigFn_fnState_init(TrackQueue *track_queue, EventQueue *eq) {
	state.queue = track_queue;
	state.evt_sq = EventQueue_connect(eq, 5);
}

/* #endregion */


