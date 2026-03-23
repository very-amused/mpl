#pragma once
#include "backend.h"

/* Linux AudioBackends */
extern AudioBackend AB_PulseAudio;
extern AudioBackend AB_Pipewire;

/* Windows AudioBackend */
extern AudioBackend AB_WASAPI;

/* Fake Audio Server for Testing */
extern AudioBackend AB_FAST;

/* Get default AudioBackend */
AudioBackend *AB_Default();
