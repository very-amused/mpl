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

#include <fcntl.h>
#include <windows.h>
#include <combaseapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <winerror.h>

// CoreAudio backend context
typedef struct Ctx {
	// Event queue for communicating with UI
	EventQueue *evt_queue;

	// Audio device enumerator (used to find the default audio device)
	struct IMMDeviceEnumerator *audiodev_enum;
	// Audio device we're sending to
	IMMDevice *audio_device;
	// Audio + AudioRender clients, these collectively fulfill the role of an audio stream.
	// The AudioClient holds ownership over the buffer *data* (read only),
	// while the AudioRenderClient holds ownership over the buffer *write methods* and *callbacks*
	IAudioClient *audio_client;
	IAudioRenderClient *render_client;

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


	// Connect to event queue, store settings
	ctx->evt_queue = EventQueue_connect(eq, O_WRONLY);
	if (!ctx->evt_queue) {
		return AudioBackend_BAD_ALLOC;
	}
	ctx->settings = settings;

	// Instantiate audio device enumerator
	HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, &IID_IMMDeviceEnumerator,
			(void **)&ctx->audiodev_enum);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to create audio device enumerator\n");
		deinit(ctx);
		return AudioBackend_BAD_ALLOC;
	}
	// Get default audio output device
	hr = ctx->audiodev_enum->lpVtbl->GetDefaultAudioEndpoint(
			ctx->audiodev_enum, eRender, eConsole, &ctx->audio_device);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to get default audio endpoint\n");
		deinit(ctx);
		return AudioBackend_CONNECT_ERR;
	}

	// Initialize audio client/session (in a paused state w/ no format configured)
	hr = ctx->audio_device->lpVtbl->Activate(ctx->audio_device, &IID_IAudioClient, CLSCTX_ALL,
			NULL, (void **)&ctx->audio_client);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to create audio client\n");
		deinit(ctx);
		return  AudioBackend_CONNECT_ERR;
	}

	
	return AudioBackend_OK;
}


static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;

	HRESULT hr = ctx->audio_client->lpVtbl->Stop(ctx->audio_client);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to stop AudioClient\n");
	}

	// Call a windows COM destructor from C
#define RELEASE(obj) \
	if (obj) { \
		obj->lpVtbl->Release(obj); \
		obj = NULL; \
	}

	RELEASE(ctx->render_client);
	RELEASE(ctx->audio_client);
	RELEASE(ctx->audio_device);
	RELEASE(ctx->audiodev_enum);

#undef RELEASE
}
