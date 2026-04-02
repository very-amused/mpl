#pragma once
#include "backend.h"
#include "config/settings.h"

/* Linux AudioBackends */
extern AudioBackend AB_PulseAudio;
extern AudioBackend AB_Pipewire;

/* Windows AudioBackend */
extern AudioBackend AB_WASAPI;

/* Fake Audio Server for Testing */
extern AudioBackend AB_FAST;

/* Get AudioBackend set by settings->audio_backend,
 * fall back to AB_Default() if unset or invalid */
AudioBackend *AB_Configured(const Settings *settings);
/* Get default AudioBackend */
AudioBackend *AB_Default();
