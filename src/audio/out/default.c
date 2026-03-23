#include "backend.h"
#include "backends.h"

#include <assert.h>
#include <stdbool.h>

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
