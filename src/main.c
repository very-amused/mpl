#include "config/config.h"
#include "config/internal/state.h"
#include "config/keybind/keybind.h"
#include "config/keybind/keybind_map.h"
#include "error.h"
#include "queue/queue.h"
#include "track.h"
#include "ui/cli.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "ui/timecode.h"
#include "util/log.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

int main(int argc, const char **argv) {
	printf("This is MPL v%s\n", MPL_VERSION);

	// Parse CLI args 
	if (argc < 2) {
		fprintf(stderr, "usage: mpl [-v] [-vv] {file}\n");
		return 1;
	}
	UserInterface_CLI_opts ui_opts;
	UserInterface_CLI_opts_parse(&ui_opts, argc, argv);
	configure_av_log(); // Configure libav logging


	// Fire up CLI user interface and the main EventQueue
	UserInterface_CLI ui;
	UserInterface_CLI_init(&ui, &ui_opts);
	LOG(Verbosity_VERBOSE, "Logging enabled: %s\n", Verbosity_name(CLI_opts.verbosity));

	// Parse mpl.conf
	mplConfig config;
	char *config_path = NULL;
	size_t config_path_len;
	if (mplConfig_find_path(&config_path, &config_path_len) == 0) {
		mplConfig_parse(&config, config_path);
	} else {
		// TODO: mplConfig_init_defaults()
		LOG(Verbosity_NORMAL, "Missing default config! exiting.\n");
		exit(1);
	}
	free(config_path);

	// Form URL from file argv
	const char *file = argv[argc-1];
	static const char LIBAV_PROTO_FILE[] = "file:";
	static const size_t LIBAV_PROTO_FILE_LEN = sizeof(LIBAV_PROTO_FILE);
	const size_t url_len = LIBAV_PROTO_FILE_LEN + strlen(file);
	char *url = malloc((url_len + 1) * sizeof(char));
	snprintf(url, url_len, "%s%s", LIBAV_PROTO_FILE, file);

	// Initialize queue w/ track
	Queue queue;
	Queue_init(&queue);
	Queue_connect_audio(&queue, NULL, ui.evt_queue);
	if (Queue_prepend(&queue, Track_new(url, url_len)) != 0) {
		goto quit;
	}
	Queue_play(&queue, 0);

	// Display metadata
	const Track *track = Queue_cur_track(&queue);
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

	// WIP: Initialize configState so bindings work
	configState_init(&queue, ui.evt_queue);

	// Handle events on the main thread
	Event evt;
	while (EventQueue_recv(ui.evt_queue, &evt) == 0) {
		switch (evt.event_type) {
		case mpl_QUIT:
			LOG(Verbosity_DEBUG, "Quitting from mpl_QUIT\n");
			goto quit;
		case mpl_KEYPRESS:
		{
			EventBody_Keypress key = evt.body_inline;
			enum Keybind_ERR err = KeybindMap_call_keybind(config.keybinds, key);
			if (err != Keybind_OK) {
				LOG(Verbosity_VERBOSE, "Keybind error: %s\n", Keybind_ERR_name(err));
			} else {
				break;
			}
			if (call_keybind(key) != 0) {
				LOG(Verbosity_VERBOSE, "Unknown key: %lc\n", key);
			}
			break;
		}
		case mpl_TIMECODE:
		{
			EventBody_Timecode timecode = evt.body_inline;
			const Track *tr = Queue_cur_track(&queue);
			AudioPCM pcm = tr->audio->pcm;
			char timecode_buf[255];
			char duration_buf[255];
			fmt_timecode(timecode_buf, sizeof(timecode_buf), timecode, &pcm);
			fmt_timecode(duration_buf, sizeof(duration_buf), tr->audio->duration_timecode, &pcm);
			static const char CLEAR_LINE_VT100[] = "\033[2K\r";
			fprintf(stderr, CLEAR_LINE_VT100);
			fprintf(stderr, "%s/%s", timecode_buf, duration_buf);
			break;
		}
		case mpl_TRACK_END:
		{
			const Track *next = Queue_next_track(&queue);
			if (!next) {
				goto quit;
			}
		}
		default:
			fprintf(stderr, "Warning: unhandled event %s\n", MPL_EVENT_name(evt.event_type));
		}
	}
quit:

	// Cleanup
	LOG(Verbosity_DEBUG, "Deinitializing UI\n");
	UserInterface_CLI_deinit(&ui);
	LOG(Verbosity_DEBUG, "Deinitializing Queue\n");
	Queue_deinit(&queue);
	free(url);
	LOG(Verbosity_DEBUG, "Deinitializing config\n");
	mplConfig_deinit(&config);

	return 0;
}
