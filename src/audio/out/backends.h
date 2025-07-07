#pragma once
#include "backend.h"

extern AudioBackend AB_PulseAudio;
extern AudioBackend AB_Pipewire;

// AudioBackend that reads from an AudioBuffer at a fixed rate and does nothing else
extern AudioBackend AB_dummy;