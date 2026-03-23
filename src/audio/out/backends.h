#pragma once
#include "backend.h"

/* Linux AudioBackends */
extern AudioBackend AB_PulseAudio;
extern AudioBackend AB_Pipewire;

/* Windows AudioBackends */
extern AudioBackend AB_CoreAudio;

/* Fake Audio Server for Testing */
extern AudioBackend AB_FAST;

/* Get default AudioBackend */
AudioBackend *AB_Default();
