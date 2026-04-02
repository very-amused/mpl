#include "backend.h"
#include "backends.h"
#include "error.h"
#include "util/log.h"

#include <assert.h>
#include <stdbool.h>

AudioBackend *AB_Configured(const Settings *settings) {
	const char *name = settings->audio_backend;
	AudioBackend *def = AB_Default();
	if (!name) {
		LOG(Verbosity_VERBOSE, "audio_backend setting is unset, using default AudioBackend (%s)\n", def->name);
		return def;
	}

	static const char ERR_UNSUPPORTED[] = "audio_backend '%s' is unsupported on this build, falling back to default (%s)\n";

	if (strcmp(settings->audio_backend, "pulseaudio")) {
#if defined(AO_PULSEAUDIO)
		return &AB_PulseAudio;
#else
		LOG(Verbosity_NORMAL, ERR_UNSUPPORTED, "pulseaudio", def->name);
		return def;
#endif
	}
	if (strcmp(settings->audio_backend, "pipewire")) {
#if defined(AO_PIPEWIRE)
		return &AB_Pipewire;
#else
		LOG(Verbosity_NORMAL, ERR_UNSUPPORTED, "pipewire", def->name);
		return def;
#endif
	}

	if (strcmp(settings->audio_backend, "wasapi")) {
#if defined(AO_WASAPI)
		return &AB_WASAPI;
#else
		LOG(Verbosity_NORMAL, ERR_UNSUPPORTED, "wasapi", def->name);
		return def;
#endif
	}

	if (strcmp(settings->audio_backend, "fast")) {
#if defined(AO_FAST)
		return &AB_FAST;
#else
		LOG(Verbosity_NORMAL, ERR_UNSUPPORTED, "fast", def->name);
		return def;
#endif
	}

	LOG(Verbosity_NORMAL, "Unknown audio_backend '%s', falling back to default (%s)\n", settings->audio_backend, def->name);
	return def;
}

AudioBackend *AB_Default() {
#if defined(AO_FAST)
	return &AB_FAST;
#elif defined(AO_PULSEAUDIO)
	return &AB_PulseAudio;
#elif defined(AO_WASAPI)
	return &AB_WASAPI;
#else
	static_assert(false, "Could not find a supported AudioBackend. Consider enabling ao_fast (emulated audio output) if developing for a new platform.");
	return NULL;
#endif
}
