#include "config/keybind/keybind_map.h"
#include "error.h"
#include "interface.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "ui/input_thread.h"
#include "ui/timecode.h"
#include "util/log.h"
#include <string.h>

// CLI context
typedef struct Ctx {
	InputThread *input_thread;	
	const Settings *settings;
	const Track *cur_track;
	EventBody_Timecode track_pos;
} Ctx;

// UserInterface methods
static enum UserInterface_ERR init(void *ud, EventQueue *evt_queue, const Settings *settings);
static void deinit(void *ud);
static enum UserInterface_ERR mainloop(void *ud, EventQueue *evt_queue,
		Queue *track_queue, Config *config);

// UI_Control methods
static enum UserInterface_ERR refresh_timecode(void *ud);
static enum UserInterface_ERR refresh_metadata(void *ud);
//static enum UserInterface_ERR show_config(void *ud);

static UI_Control ctrl = {0};

UserInterface UI_CommandLine = {
	.name = "Command Line Interface",
	.ctrl = &ctrl,

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
	ctx->settings = settings;

	// Register UI control methods
	ctrl.refresh_timecode = refresh_timecode;
	ctrl.refresh_metadata = refresh_metadata;

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

static enum UserInterface_ERR mainloop(void *ud, EventQueue *evt_queue,
		Queue *track_queue, Config *config) {
	Ctx *ctx = ud;

	// Handle if there's a track loaded in the queue
	const Track *track = Queue_cur_track(track_queue);
	if (track) {
		ctx->cur_track = track;
		refresh_metadata(ctx);
		Queue_play(track_queue, 0);
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
			ctx->track_pos = evt.body_inline;
			refresh_timecode(ctx);
			break;
		}

		case mpl_TRACK_END:
		{
			const Track *next = Queue_next_track(track_queue);
			if (!next) {
				return 0;
			}
			ctx->cur_track = next;
			// TODO: implement track switching
			// (we need a mpl_TRACK_BUFFERED event to initiate prebuffering of the next track in the queue)
		}

		default:
			fprintf(stderr, "Warning: unhandled event %s\n", MPL_EVENT_name(evt.event_type));
		}
	}
		
	return UserInterface_EVENT_QUEUE_EOF;
}

/* UI_Control methods */

static enum UserInterface_ERR refresh_timecode(void *ud) {
	Ctx *ctx = ud;
	const Track *track = ctx->cur_track;
	if (!track) {
		return UserInterface_NO_TRACK;
	}

	const AudioPCM pcm = ctx->cur_track->audio->buf_pcm;
	const bool show_ms = ctx->settings->ui_timecode_ms;

	static char timecode_buf[255];
	static char duration_buf[255];
	fmt_timecode(timecode_buf, sizeof(timecode_buf), ctx->track_pos, &pcm, show_ms);
	fmt_timecode(duration_buf, sizeof(duration_buf), ctx->cur_track->audio->duration_timecode, &pcm, show_ms);

	static const char CLEAR_LINE_VT100[] = "\033[2K\r";
	fprintf(stderr, CLEAR_LINE_VT100);
	fprintf(stderr, "%s/%s", timecode_buf, duration_buf);

	return UserInterface_OK;
}

static enum UserInterface_ERR refresh_metadata(void *ud) {
	Ctx *ctx = ud;
	const Track *track = ctx->cur_track;
	if (!track) {
		return UserInterface_NO_TRACK;
	}

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

	return UserInterface_OK;
}
