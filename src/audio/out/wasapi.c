#include "audio/buffer.h"
#include "audio/out/wasapi_fb_thread.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include "backend.h"
#include "backends.h"
#include "config/settings.h"
#include "error.h"
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
	// Framebuffer loop thread (we get this from other audio APIs, but need to provide our own with WASAPI)
	WASAPI_fbThread *framebuffer_thread;

	// Playback buffer for current and next audio track
	AudioBuffer *playback_buffer;
	AudioBuffer *next_buffer;

	// Configuration from mpl.conf
	const Settings *settings;
} Ctx;

/* WASAPI AudioBackend methods */
static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings);
static void deinit(void *ctx__);
static bool negotiate_pcm(void *ctx__, AudioPCM *dst_pcm, const AudioPCM *src_pcm);
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

	.negotiate_pcm = negotiate_pcm,

	.prepare = prepare,
	.play = play,

	.lock = lock,
	.unlock = unlock,
	.seek = seek,

	.ctx_size = sizeof(Ctx)
};

/* WASASPI callbacks. *userdata is of type *Ctx. */
static void wasapi_write_cb_(void *userdata);

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

	// Activate stream so AudioTracks can perform PCM negotiation
	hr = ctx->audio_device->lpVtbl->Activate(ctx->audio_device, &IID_IAudioClient, CLSCTX_ALL,
			NULL, (void **)&ctx->stream);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to activate audio stream\n");
		return AudioBackend_STREAM_ERR;
	}

	return AudioBackend_OK;
}


static void deinit(void *ctx__) {
	Ctx *ctx = ctx__;

	if (ctx->framebuffer_thread) {
		WASAPI_fbThread_free(ctx->framebuffer_thread);
	}

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


// Return true if we need to resample
static bool negotiate_pcm(void *ctx__, AudioPCM *dst_pcm, const AudioPCM *src_pcm) {
	Ctx *ctx = ctx__;

	// Verify if WASAPI will accept our sample format as is
	const WAVEFORMATEXTENSIBLE wavfmt = AudioPCM_wasapi_waveformat(src_pcm);
	WAVEFORMATEX *alt_fmt = NULL;
	// TODO support WASAPI exclusive mode
	HRESULT hr = ctx->stream->lpVtbl->IsFormatSupported(ctx->stream, AUDCLNT_SHAREMODE_SHARED, &(wavfmt.Format), &alt_fmt);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to verify sample format with WASAPI\n");
		return false;
	}
	if (!alt_fmt) {
		// WASAPI will accept our PCM as-is. This is the ideal scenario.
		return false;
	}

	// We need to resample to the format WASAPI has given us as a "nearest match"
	int status = AudioPCM_from_wasapi_waveformat(dst_pcm, alt_fmt);
	if (status != 0) {
		// We return false so AudioClient::Initialize will later fail.
		// This is a more predictable fail state than if the caller tries to resample
		// to an invalid AudioPCM destination format
		LOG(Verbosity_VERBOSE, "Unable to convert WAVEFORMATEX to AudioPCM\n");
		return false;
	}
	return true;
}


static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *t) {
	Ctx *ctx = ctx__;

	// If stream is initialized, the caller must use queue() instead
	HRESULT hr;
	if (ctx->stream) {
		REFERENCE_TIME dontcare;
		hr = ctx->stream->lpVtbl->GetStreamLatency(ctx->stream, &dontcare);
		if (!(hr == AUDCLNT_E_NOT_INITIALIZED)) {
			return AudioBackend_STREAM_EXISTS;
		}
	} else {
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


	// TODO: support WASAPI exclusive mode
	static const AUDCLNT_SHAREMODE sharemode = AUDCLNT_SHAREMODE_SHARED;

	// Verify WASAPI will accept our sample format
	// (the client should have called pcm_negotiate to figure this out BEFORE calling prepare)
	WAVEFORMATEXTENSIBLE wavfmt = AudioPCM_wasapi_waveformat(&t->buf_pcm);
	WAVEFORMATEX *alternate_fmt = NULL;
	hr = ctx->stream->lpVtbl->IsFormatSupported(ctx->stream, sharemode, &(wavfmt.Format), &alternate_fmt);
	if (hr != S_OK && wavfmt.Format.nChannels <= 2) {
		// try again with a non-extended basic WAVEFORMATEX
		// sometimes this is needed
		// ref https://learn.microsoft.com/en-us/windows/win32/coreaudio/device-formats
		if (AudioPCM_wasapi_waveformat_simplify(&wavfmt) == 0) {
			hr = ctx->stream->lpVtbl->IsFormatSupported(ctx->stream, sharemode, &(wavfmt.Format), &alternate_fmt);
		}
	}
	if (hr != S_OK) {
		LOG(Verbosity_NORMAL, "prepare is failing with BAD_PCM_FMT\n");
		// TODO: figure out how to perror HRESULT's
		return AudioBackend_BAD_PCM_FMT;
	}


	// Initialize stream
	// TODO: support WASAPI exclusive mode
	const DWORD STREAM_FLAGS = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
	const REFERENCE_TIME hns_buf_duration = ctx->settings->ab_buffer_ms * 10000; // 1 ms = 10000 * 100ns
	hr = ctx->stream->lpVtbl->Initialize(ctx->stream,
			sharemode,
			STREAM_FLAGS,
			hns_buf_duration,
			0,
			&(wavfmt.Format),
			session_guid);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to initialize audio stream: 0x%lx\n", hr);
		return AudioBackend_STREAM_ERR;
	}

	LOG(Verbosity_VERBOSE, "AudioClient successfully initialized!\n");


	// Set up write callback on its own thread
	ctx->framebuffer_thread = WASAPI_fbThread_new();
	if (!ctx->framebuffer_thread) {
		return AudioBackend_BAD_ALLOC;
	}
	WASAPI_fbThread_set_write_cb(ctx->framebuffer_thread, wasapi_write_cb_, ctx);
	ctx->stream->lpVtbl->SetEventHandle(ctx->stream,
			WASAPI_fbThread_get_write_evt_handle(ctx->framebuffer_thread));
	if (WASAPI_fbThread_start(ctx->framebuffer_thread) != 0) {
		return AudioBackend_WASAPI_FBTHREAD_ERR;
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
		return AudioBackend_WASAPI_ERR;
	}

	// Connect playback buffer to framebuffer
	ctx->playback_buffer = t->buffer;

	// Get max number of frames we can write
	uint32_t max_frame_count;
	hr = ctx->stream->lpVtbl->GetBufferSize(ctx->stream, &max_frame_count);
	if (FAILED(hr)) {
		return AudioBackend_WASAPI_ERR;
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
	LOG(Verbosity_VERBOSE, "Prefilled WASAPI buffer with %d frames (%zu bytes)\n", actual_frame_count, bytes_read);
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

static void wasapi_write_cb_(void *userdata) {
	Ctx *ctx = userdata;

	// Get max number of frames we can write
	uint32_t max_frame_count;
	HRESULT hr = ctx->stream->lpVtbl->GetBufferSize(ctx->stream, &max_frame_count);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to get max frame count from WASAPI\n");
		return;
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
		// this path is normal and occurs pretty frequently
		// WASAPI will just wait until it has enough buffer space
		return;
	}
	size_t bytes_read = AudioBuffer_read(ctx->playback_buffer, tb, frame_count * frame_size, true);
	const uint32_t actual_frame_count = bytes_read / frame_size;

	// Release the transfer buffer to WASAPI
	hr = ctx->stream_render->lpVtbl->ReleaseBuffer(ctx->stream_render, actual_frame_count, 0);
	if (FAILED(hr)) {
		LOG(Verbosity_VERBOSE, "Failed to release transfer buffer to WASAPI\n");
	}

	LOG(Verbosity_DEBUG, "Wrote %zu bytes to WASAPI\n", bytes_read);
}
