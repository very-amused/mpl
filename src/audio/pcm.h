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

#ifdef AO_PULSEAUDIO
#include <pulse/sample.h>
#include <pulse/channelmap.h>
#include <pulse/def.h>

#define MPL_PA_BUF_MS 100

pa_sample_spec AudioPCM_pulseaudio_spec(const AudioPCM *pcm);
pa_channel_map AudioPCM_pulseaudio_channel_map(const AudioPCM *pcm);
pa_buffer_attr AudioPCM_pulseaudio_buffer_attr(const AudioPCM *pcm);
#endif
