#include "config/keybind/keybind_map.h"
#include "config/parse_v2/lexer.h"
#include "config/parse_v2/parser.h"
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
	TermIOThread *input_thread;	
} Ctx;

// UserInterface methods
static enum UserInterface_ERR init(void *ud, EventQueue *evt_queue, Config *config);
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

static enum UserInterface_ERR init(void *ud, EventQueue *evt_queue, Config *config) {
	Ctx *ctx = ud;
	memset(ctx, 0, sizeof(Ctx));

	ctx->input_thread = TermIOThread_new(evt_queue, config->keybinds);
	if (!ctx->input_thread) {
		return UserInterface_BAD_ALLOC;
	}

	return UserInterface_OK;
}

static void deinit(void *ud) {
	Ctx *ctx = ud;
	if (ctx->input_thread) {
		TermIOThread_free(ctx->input_thread);
		ctx->input_thread = NULL;
	}
}

/* Mainloop for CLI */

// Print track timecode and duration
static void refresh_timecode(EventBody_Timecode timecode, const AudioTrack *audio, const Settings *settings, TermIOThread *thr);
// Print track metadata
static void refresh_metadata(const TrackMeta *meta);

static enum UserInterface_ERR mainloop(void * ctx__,
		EventQueue *evt_queue, TrackQueue *track_queue, Config *config) {
	Ctx *ctx = ctx__;

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
			}
			break;

		case mpl_INPUT_LINE:
			{
				EventBody_InputLine line = evt.body;
				enum Parser_ERR err = Lexer_tokenize(config->lexer, line);
				free(line);
				if (err != Parser_OK) {
					LOG(Verbosity_NORMAL, "Parse error: %s\n", Parser_ERR_name(err));
					break;
				}

				Parser_LineError parse_err;
				ParseNode *stmt = Parser_parse_ShellStmt(config->parser, &parse_err);
				if (parse_err.type != Parser_OK) {
					if (parse_err.strerr) {
						LOG(Verbosity_NORMAL, "Parse error: %s\n", parse_err.strerr);
					} else {
						LOG(Verbosity_NORMAL, "Parse error: %s\n", Parser_ERR_name(err));
					}
					Parser_LineError_deinit(&parse_err);
					ParseNode_rfree(stmt);
					break;
				}
				err = Parser_walk(config->parser, config, Parser_WALK_KEYBINDS | Parser_WALK_FUNCTIONS | Parser_WALK_MACROS, stmt);
				if (err != Parser_OK) {
					LOG(Verbosity_NORMAL, "Error: %s\n", Parser_ERR_name(err));
				}
				ParseNode_rfree(stmt);
			}
			break;

		case mpl_TIMECODE:
			refresh_timecode(evt.body_inline, &TrackQueue_cur_track(track_queue)->audio, &config->settings, ctx->input_thread);
			break;
	
		case mpl_TRACK_META:
			refresh_metadata(evt.body);
			free(evt.body);
			break;

		case mpl_TRACK_END:
			{
				const Track *next = TrackQueue_next_track(track_queue);
				if (!next) {
					return 0;
				}
				// TODO: implement track switching
				// (we need a mpl_TRACK_BUFFERED event to initiate prebuffering of the next track in the queue)
			}
			break;

		case mpl_SHELL_OPEN:
			{
				TermIOThread_set_mode(ctx->input_thread, InputMode_SHELL);
			}
			break;

		case mpl_SHELL_CLOSE:
			{
				TermIOThread_set_mode(ctx->input_thread, InputMode_KEY);
			}
			break;

		default:
			fprintf(stderr, "Warning: unhandled event %s\n", MPL_EVENT_name(evt.event_type));
		}
	}
		
	return UserInterface_EVENT_QUEUE_EOF;
}

static void refresh_timecode(EventBody_Timecode timecode,
		const AudioTrack *audio, const Settings *settings,
		TermIOThread *thr) {
	const AudioPCM pcm = audio->buf_pcm;
	const bool show_ms = settings->ui_timecode_ms;

	static char timecode_buf[255];
	static char duration_buf[255];
	fmt_timecode(timecode_buf, sizeof(timecode_buf), timecode, &pcm, show_ms);
	fmt_timecode(duration_buf, sizeof(duration_buf), audio->duration_timecode, &pcm, show_ms);

	TermIOThread_update_timecode(thr, timecode_buf, duration_buf);
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
