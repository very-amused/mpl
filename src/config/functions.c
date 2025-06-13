#include "functions.h"
#include "audio/seek.h"
#include "config.h"
#include "config/internal/state.h"
#include "queue/queue.h"
#include "queue/state.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "util/strtokn.h"
#include "util/log.h"


/* Bindable functions */

static struct configState config_state;

void configState_init(Queue *track_queue, EventQueue *evt_queue) {
	config_state.queue = track_queue;
	config_state.evt_queue = evt_queue;
}

void play_toggle(void * _) {
	const bool pause = config_state.queue->playback_state == Queue_PLAYING;
	Queue_play(config_state.queue, pause);
}

void quit(void * _) {
	static const Event quit_evt = {
		.event_type = mpl_QUIT,
		.body_size = 0
	};
	EventQueue_send(config_state.evt_queue, &quit_evt);
}

void seek(const struct seekArgs *args) {
	Queue_seek(config_state.queue, args->ms, AudioSeek_Relative);
}

void seek_snap(const struct seekArgs *args) {
	Queue_seek_snap(config_state.queue, args->ms);
}

/* Direct eval functions */

static struct evalState eval_state;

void evalState_init(mplConfig *config) {
	eval_state.config = config;
}

// FIXME: we can make this a lot simpler by adding some basic flags on mplConfig_parse_line in config.c, namely a stmt_type binary OR flag
void include_default_keybinds(void * _) {
	KeybindMap *keybinds = eval_state.config->keybinds;
	// Set up parsing for default config
	const char *default_config = get_default_config();
	const size_t default_config_len = get_default_config_len();
	StrtoknState parse_state;
	strtokn_init(&parse_state, default_config, default_config_len);
	// Copy into mutable buffer so we can null-terminate
	char *lines = strndup(default_config, default_config_len);

	// Parse keybinds from default mpl.conf
	int lineno = 1;
	while (strtokn(&parse_state, "\n") != -1) {
		// Null terminate
		lines[parse_state.offset + parse_state.tok_len] = '\0';

		// Handle keybind lines
		static const char KEYBIND_PREFIX[] = "bind";
		if (strncmp(&lines[parse_state.offset], KEYBIND_PREFIX, sizeof(KEYBIND_PREFIX)-1) != 0) {
			lineno++;
			continue;
		}
		enum Keybind_ERR err = KeybindMap_parse_mapping(keybinds, &lines[parse_state.offset]);
		if (err != Keybind_OK) {
			LOG(Verbosity_NORMAL, "Error: line %d of default mpl.conf contains an invalid keybind: %s. This error should never be seen!\n",
					lineno, Keybind_ERR_name(err));
		}

		lineno++;
		continue;
	}

	free(lines);
}
