#include "functions.h"
#include "config/internal/state.h"
#include "queue/queue.h"
#include "queue/state.h"
#include "ui/event.h"
#include "ui/event_queue.h"

static struct configState config_state;

void configState_init(Queue *track_queue, EventQueue *evt_queue) {
	config_state.queue = track_queue;
	config_state.evt_queue = evt_queue;
}

void play_toggle() {
	const bool pause = config_state.queue->playback_state == Queue_PLAYING;
	Queue_play(config_state.queue, pause);
}

void quit() {
	static const Event quit_evt = {
		.event_type = mpl_QUIT,
		.body_size = 0
	};
	EventQueue_send(config_state.evt_queue, &quit_evt);
}
