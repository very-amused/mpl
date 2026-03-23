#include <fcntl.h>
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

#include <windows.h>
#include <combaseapi.h>
#include <mmdeviceapi.h>
#include <winerror.h>
#include <unknwn.h>

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

static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings) {
	Ctx *ctx = ctx__;

	// Call a windows COM destructor from C
#define RELEASE(obj) \
	if (obj) { \
		obj->lpVtbl->Release(obj); \
		obj = NULL; \
	}

	// Connect to event queue, store settings
	ctx->evt_queue = EventQueue_connect(eq, O_WRONLY);
	if (!ctx->evt_queue) {
		return AudioBackend_BAD_ALLOC;
	}
	ctx->settings = settings;

	// Instantiate audio device enumerator
	IMMDeviceEnumerator *audiodev_enum;
	HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, &IID_IMMDeviceEnumerator,
			(void **)&audiodev_enum);
	if (FAILED(hr)) {
		RELEASE(audiodev_enum);
		return AudioBackend_BAD_ALLOC;
	}

	// TODO
	return AudioBackend_OK;
}
