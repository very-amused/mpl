#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"
#include "backends.h"
#include "config/settings.h"
#include "error.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "util/log.h"

// CoreAudio backend context
typedef struct Ctx {
	// Event queue for communicating with UI
	EventQueue *evt_queue;

	// TODO

	// Playback buffer for current and next audio track
	AudioBuffer *playback_buffer;
	AudioBuffer *next_buffer;

	// Configuration from mpl.conf
	const Settings *settings;
} Ctx;

/* CoreAudio AudioBackend methods */
static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings);
static void deinit(void *ctx__);
static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track);
static enum AudioBackend_ERR play(void *ctx__, bool pause);
static void lock(void *ctx__);
static void unlock(void *ctx__);
static void seek(void *ctx__);

/* AudioBackend implementation using CoreAudio */
AudioBackend AB_CoreAudio = {
	.name = "CoreAudio",

	.init = init,
	.deinit = deinit,

	.prepare = prepare,
	.play = play,

	.lock = lock,
	.unlock = unlock,
	.seek = seek,

	.ctx_size = sizeof(Ctx)
};
