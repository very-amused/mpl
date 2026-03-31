#include "cli.h"
#include "config/internal/state.h"
#include "config/settings.h"
#include "queue/queue.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "ui/input_thread.h"
#include "ui/timecode.h"
#include "util/log.h"
#include <string.h>

int UserInterface_CLI_init(UserInterface_CLI *ui) {
	// Zero pointers so we can use deinit as an escape hatch
	memset(ui, 0, sizeof(UserInterface_CLI));

	// Initialize event queue
	ui->evt_queue = EventQueue_new();
	if (!ui->evt_queue) {
		UserInterface_CLI_deinit(ui);
		return 1;
	}
	// Initialize input thread
	ui->input_thread = InputThread_new(ui->evt_queue);
	if (!ui->input_thread) {
		UserInterface_CLI_deinit(ui);
		return 1;
	}

	return 0;
}

void UserInterface_CLI_deinit(UserInterface_CLI *ui) {
	InputThread_free(ui->input_thread);
	LOG(Verbosity_DEBUG, "InputThread_free finished\n");
	EventQueue_free(ui->evt_queue);
}

static void UserInterface_CLI_display_metadata(const Track *track) {
	// Display track metadata
	static const char TERM_BOLD[] = "\x1b[1m";
	static const char TERM_ITAL[] = "\x1b[3m";
	static const char TERM_RESET[] = "\x1b[0m";
	if (track->meta.artist) {
		fprintf(stderr, "%sArtist:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, track->meta.artist, TERM_RESET);
	}
	if (track->meta.name) {
		fprintf(stderr, "%sTitle:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, track->meta.name, TERM_RESET);
	}
	if (track->meta.album) {
		fprintf(stderr, "%sAlbum:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, track->meta.album, TERM_RESET);
	}
}

static void UserInterface_CLI_display_timecode(const EventBody_Timecode timecode, const Track *cur_track, const Settings *settings) {
	AudioPCM pcm = cur_track->audio->buf_pcm;
	const bool show_ms = settings->ui_timecode_ms;

	static char timecode_buf[255];
	static char duration_buf[255];
	fmt_timecode(timecode_buf, sizeof(timecode_buf), timecode, &pcm, show_ms);
	fmt_timecode(duration_buf, sizeof(duration_buf), cur_track->audio->duration_timecode, &pcm, show_ms);

	static const char CLEAR_LINE_VT100[] = "\033[2K\r";
	fprintf(stderr, CLEAR_LINE_VT100);
	fprintf(stderr, "%s/%s", timecode_buf, duration_buf);
}

int UserInterface_CLI_mainloop(UserInterface_CLI *ui, Queue *queue, Config *config) {
	// Play the track provided via argv
	const Track *track = Queue_cur_track(queue);
	if (track) {
		UserInterface_CLI_display_metadata(track);
		Queue_play(queue, 0);
	}

	// Handle events on the main thread
	Event evt;
	while (EventQueue_recv(ui->evt_queue, &evt) == 0) {
		switch (evt.event_type) {
		case mpl_QUIT:
			LOG(Verbosity_DEBUG, "Quitting from mpl_QUIT\n");
			return 0;

		case mpl_KEYPRESS:
		{
			EventBody_Keypress key = evt.body_inline;
			LOG(Verbosity_DEBUG, "Pulled keypress `%c` from EventQueue\n", key);
			enum Keybind_ERR err = KeybindMap_call_keybind(config->keybinds, key);
			if (err != Keybind_OK) {
				LOG(Verbosity_VERBOSE, "Keybind error: %s\n", Keybind_ERR_name(err));
			}
			break;
		}

		case mpl_TIMECODE:
		{
			UserInterface_CLI_display_timecode(evt.body_inline, Queue_cur_track(queue), &config->settings);
			break;
		}

		case mpl_TRACK_END:
		{
			const Track *next = Queue_next_track(queue);
			if (!next) {
				return 0;
			}
			// TODO: implement track switching
		}

		default:
			fprintf(stderr, "Warning: unhandled event %s\n", MPL_EVENT_name(evt.event_type));
		}
	}

	return EOF;
}
