#include "functions.h"
#include "config/internal/state.h"
#include "queue/queue.h"
#include "queue/state.h"

void play_toggle() {
	const bool pause = config_state.queue->playback_state == Queue_PLAYING;
	Queue_play(config_state.queue, pause);
}
