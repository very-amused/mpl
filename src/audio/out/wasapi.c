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
#include "error.h"

#include <errhandlingapi.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdint.h>
#include <windows.h>
#include <initguid.h>
#include <combaseapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <ksmedia.h>
#include <winerror.h>

#include <assert.h>

// WASAPI backend context
typedef struct Ctx {
	// Event queue for communicating with UI
	EventQueue *evt_queue;

	// Audio device enumerator (used to find the default audio device)
	struct IMMDeviceEnumerator *audiodev_enum;
	// Audio device we're sending to
	IMMDevice *audio_device;
	// Audio stream for current and next tracks
	IAudioClient *stream;
	IAudioRenderClient *stream_render;
	IAudioClient *next_stream;
	IAudioRenderClient *next_stream_render;
	// Audio session for grouping streams w/ metadata
	IAudioSessionControl *session;

	// Playback buffer for current and next audio track
	AudioBuffer *playback_buffer;
	AudioBuffer *next_buffer;

	// Configuration from mpl.conf
	const Settings *settings;
} Ctx;

/* WASAPI AudioBackend methods */
static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings);
static void deinit(void *ctx__);
static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track);
static enum AudioBackend_ERR play(void *ctx__, bool pause);
static void lock(void *ctx__);
static void unlock(void *ctx__);
static void seek(void *ctx__);

/* AudioBackend implementation using WASAPI */
AudioBackend AB_WASAPI = {
	.name = "WASAPI",

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

	// Initialize COM library
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

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

	assert(ctx->audio_device != NULL);
	LOG(Verbosity_DEBUG, "Successfully initialized WASAPI! ctx->audio_device=%p\n", ctx->audio_device);
	return AudioBackend_OK;
}


static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;

	if (ctx->stream) {
		HRESULT hr = ctx->stream->lpVtbl->Stop(ctx->stream);
		if (FAILED(hr)) {
			LOG(Verbosity_VERBOSE, "Failed to stop stream\n");
		}
	}
	if (ctx->next_stream) {
		HRESULT hr = ctx->next_stream->lpVtbl->Stop(ctx->next_stream);
		if (FAILED(hr)) {
			LOG(Verbosity_VERBOSE, "Failed to stop next_stream\n");
		}
	}

	// Call a windows COM destructor from C
#define RELEASE(obj) \
	if (obj) { \
		obj->lpVtbl->Release(obj); \
		obj = NULL; \
	}

	RELEASE(ctx->stream);
	RELEASE(ctx->stream_render);
	RELEASE(ctx->next_stream);
	RELEASE(ctx->next_stream_render);
	RELEASE(ctx->session);
	RELEASE(ctx->audio_device);
	RELEASE(ctx->audiodev_enum);

#undef RELEASE
}

static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *t) {
	Ctx *ctx = ctx__;
	LOG(Verbosity_DEBUG, "Breakpoint 1\n");

	// If stream is initialized, the caller must use queue() instead
	HRESULT hr;
	if (ctx->stream) {
		REFERENCE_TIME dontcare;
		hr = ctx->stream->lpVtbl->GetStreamLatency(ctx->stream, &dontcare);
		if (!(hr == AUDCLNT_E_NOT_INITIALIZED || hr == AUDCLNT_E_DEVICE_INVALIDATED)) {
			return AudioBackend_STREAM_EXISTS;
		}
	} else {
		LOG(Verbosity_DEBUG, "Breakpoint 2\n");
		assert(ctx->audio_device != NULL);
		hr = ctx->audio_device->lpVtbl->Activate(ctx->audio_device, &IID_IAudioClient, CLSCTX_ALL,
				NULL, (void **)&ctx->stream);
		LOG(Verbosity_DEBUG, "Breakpoint 3\n");
		if (FAILED(hr)) {
			LOG(Verbosity_VERBOSE, "Failed to activate audio stream\n");
			return AudioBackend_STREAM_ERR; 
		}
	}




	// Attach this stream to an existing session if possible
	GUID *session_guid = NULL;
	if (ctx->session) {
		hr = ctx->session->lpVtbl->GetGroupingParam(ctx->session, session_guid);
		if (FAILED(hr)) {
			LOG(Verbosity_VERBOSE, "Failed to attach to existing audio session\n");
			session_guid = NULL;
		}
	}


	// Initialize stream
	// TODO: support WASAPI exclusive mode
	static const DWORD STREAM_FLAGS = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
	const WAVEFORMATEXTENSIBLE wavfmt = AudioPCM_wasapi_waveformat(&t->pcm);
	const REFERENCE_TIME hns_buf_duration = ctx->settings->ab_buffer_ms * 10000; // 1 ms = 10000 * 100ns
	hr = ctx->stream->lpVtbl->Initialize(
			ctx->stream,
			AUDCLNT_SHAREMODE_SHARED,
			STREAM_FLAGS,
			hns_buf_duration,
			0,
			&wavfmt,
			session_guid);
	if (FAILED(hr)) {
		HRESULT err_codes = {
			AUDCLNT_E_ALREADY_INITIALIZED,
			AUDCLNT_E_WRONG_ENDPOINT_TYPE,
			AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED,
			AUDCLNT_E_BUFFER_SIZE_ERROR,
			AUDCLNT_E_CPUUSAGE_EXCEEDED,
			AUDCLNT_E_DEVICE_INVALIDATED,
			AUDCLNT_E_RESOURCES_INVALIDATED,
			AUDCLNT_E_DEVICE_IN_USE,
			AUDCLNT_E_ENDPOINT_CREATE_FAILED,
			AUDCLNT_E_INVALID_DEVICE_PERIOD,
			AUDCLNT_E_UNSUPPORTED_FORMAT,
			AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED,
			AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL,
			AUDCLNT_E_SERVICE_NOT_RUNNING,
			E_POINTER,
			E_INVALIDARG,
			E_OUTOFMEMORY
		};
		if (hr == E_INVALIDARG) {
			LOG(Verbosity_DEBUG, "found it! it's error E_INVALIDARG\n");
		}
		LOG(Verbosity_VERBOSE, "Failed to initialize audio stream: 0x%lx\n", hr);
		return AudioBackend_STREAM_ERR;
	}

	// If we just created a session, save it to the AudioBackend so future streams can attach
	if (!ctx->session) {
		hr = ctx->stream->lpVtbl->GetService(ctx->stream, &IID_IAudioSessionControl, (void **)&ctx->session);
		if (FAILED(hr)) {
			LOG(Verbosity_VERBOSE, "Failed to save audio session\n");
		}
	}

	// Load render client (what we use to write frames)
	hr = ctx->stream->lpVtbl->GetService(ctx->stream, &IID_IAudioRenderClient, (void **)&ctx->stream_render);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to get audio render client\n");
		return AudioBackend_FB_WRITE_ERR;
	}

	// Get max number of frames we can write
	uint32_t max_frame_count;
	hr = ctx->stream->lpVtbl->GetBufferSize(ctx->stream, &max_frame_count);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to get WASAPI buffer size\n");
		return AudioBackend_FB_WRITE_ERR;
	}

	// Figure out how many frames we can actually write
	const size_t frame_size = ctx->playback_buffer->frame_size;
	uint32_t frame_count = AudioBuffer_max_read(ctx->playback_buffer, -1, -1, true) / frame_size;
	if (frame_count > max_frame_count) {
		frame_count = max_frame_count;
	}

	// Get a transfer buffer from WASAPI and write our frames
	BYTE *tb;
	hr = ctx->stream_render->lpVtbl->GetBuffer(ctx->stream_render, frame_count, &tb);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to get transfer buffer from WASAPI\n");
		return AudioBackend_FB_WRITE_ERR;
	}
	size_t bytes_read = AudioBuffer_read(ctx->playback_buffer, tb, frame_count * frame_size, true);
	const uint32_t actual_frame_count = bytes_read / frame_size;
	if (actual_frame_count != frame_count) {
		LOG(Verbosity_DEBUG, "Read fewer frames than expected\n");
	}

	// Release the transfer buffer to WASAPI
	hr = ctx->stream_render->lpVtbl->ReleaseBuffer(ctx->stream_render, actual_frame_count, 0);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to release transfer buffer to WASAPI\n");
	}

	return AudioBackend_OK;
}
static enum AudioBackend_ERR play(void *ctx__, bool pause) {
	Ctx *ctx = ctx__;

	HRESULT hr = pause ? ctx->stream->lpVtbl->Stop(ctx->stream) : ctx->stream->lpVtbl->Start(ctx->stream);
	return FAILED(hr) ? AudioBackend_PLAY_ERR : AudioBackend_OK;
}

static void lock(void *ctx__) {
	// WASAPI uses buffer swapping, so we don't need to worry about locking
	(void)0;
}
static void unlock(void *ctx__) {
	(void)0;
}

static void seek(void *ctx__) {
	// TODO
}
