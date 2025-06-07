#include "state.h"

struct configState config_state;

void configState_init(Queue *track_queue, EventQueue *evt_queue) {
	config_state.queue = track_queue;
	config_state.evt_queue = evt_queue;
}
