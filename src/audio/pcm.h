#pragma once
#include <stddef.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>

// Sampling parameters used to interpret decoded PCM frames
typedef struct AudioPCM {
	enum AVSampleFormat sample_fmt;
	uint32_t sample_rate;
	uint8_t n_channels;
} AudioPCM;

#ifdef HAS_AO_PULSEAUDIO
#include <pulse/sample.h>

pa_sample_spec AudioPCM_pulseaudio_spec(const AudioPCM pcm);
#endif
