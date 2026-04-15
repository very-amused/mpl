#include "config/keybind/keybind_map.h"
#include "error.h"
#include "interface.h"
#include "track_queue/queue.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "cli/input_thread.h"
#include "ui/timecode.h"
#include "util/log.h"
#include <string.h>

// CLI context
typedef struct Ctx {
	InputThread *input_thread;	
} Ctx;

// UserInterface methods
static enum UserInterface_ERR init(void *ud, EventQueue *evt_queue, const Settings *settings);
static void deinit(void *ud);
static enum UserInterface_ERR mainloop(void *ud, EventQueue *evt_queue,
		TrackQueue *track_queue, Config *config);


UserInterface UI_CommandLine = {
	.name = "Command Line Interface",

	.evt_queue = NULL,

	.init = init,
	.deinit = deinit,

	.mainloop = mainloop,

	.ctx_size = sizeof(Ctx)
};

static enum UserInterface_ERR init(void *ud, EventQueue *evt_queue, const Settings *settings) {
	Ctx *ctx = ud;
	memset(ctx, 0, sizeof(Ctx));

	ctx->input_thread = InputThread_new(evt_queue);
	if (!ctx->input_thread) {
		return UserInterface_BAD_ALLOC;
	}

	return UserInterface_OK;
}

static void deinit(void *ud) {
	Ctx *ctx = ud;
	if (ctx->input_thread) {
		InputThread_free(ctx->input_thread);
		ctx->input_thread = NULL;
	}
}

/* Mainloop for CLI */

// Print track timecode and duration
static void refresh_timecode(EventBody_Timecode timecode, const AudioTrack *audio, const Settings *settings);
// Print track metadata
static void refresh_metadata(const TrackMeta *meta);

static enum UserInterface_ERR mainloop(void * _,
		EventQueue *evt_queue, TrackQueue *track_queue, Config *config) {

	// Handle if there's a track loaded in the queue
	const Track *track = TrackQueue_cur_track(track_queue);
	if (track) {
		refresh_metadata(&track->meta);
		TrackQueue_play(track_queue, 0);
	}

	// Handle events on the main thread
	Event evt;
	while (EventQueue_recv(evt_queue, &evt) == 0) {
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
			refresh_timecode(evt.body_inline, TrackQueue_cur_track(track_queue)->audio, &config->settings);
			break;
		}
	
		case mpl_TRACK_META:
		{
			refresh_metadata(evt.body);
			free(evt.body);
			break;
		}

		case mpl_TRACK_END:
		{
			const Track *next = TrackQueue_next_track(track_queue);
			if (!next) {
				return 0;
			}
			// TODO: implement track switching
			// (we need a mpl_TRACK_BUFFERED event to initiate prebuffering of the next track in the queue)
		}

		default:
			fprintf(stderr, "Warning: unhandled event %s\n", MPL_EVENT_name(evt.event_type));
		}
	}
		
	return UserInterface_EVENT_QUEUE_EOF;
}

static void refresh_timecode(EventBody_Timecode timecode,
		const AudioTrack *audio, const Settings *settings) {
	const AudioPCM pcm = audio->buf_pcm;
	const bool show_ms = settings->ui_timecode_ms;

	static char timecode_buf[255];
	static char duration_buf[255];
	fmt_timecode(timecode_buf, sizeof(timecode_buf), timecode, &pcm, show_ms);
	fmt_timecode(duration_buf, sizeof(duration_buf), audio->duration_timecode, &pcm, show_ms);

	static const char CLEAR_LINE_VT100[] = "\033[2K\r";
	fprintf(stderr, CLEAR_LINE_VT100);
	fprintf(stderr, "%s/%s", timecode_buf, duration_buf);
}

static void refresh_metadata(const TrackMeta *meta) {
	// Display track metadata
	static const char TERM_BOLD[] = "\x1b[1m";
	static const char TERM_ITAL[] = "\x1b[3m";
	static const char TERM_RESET[] = "\x1b[0m";
	if (meta->artist) {
		fprintf(stderr, "%sArtist:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, meta->artist, TERM_RESET);
	}
	if (meta->name) {
		fprintf(stderr, "%sTitle:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, meta->name, TERM_RESET);
	}
	if (meta->album) {
		fprintf(stderr, "%sAlbum:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, meta->album, TERM_RESET);
	}
}
