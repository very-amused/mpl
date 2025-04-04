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

// Get the buffer size, in bytes, for a buffer holding {ms} milliseconds of PCM data.
uint32_t AudioPCM_buffer_size(const AudioPCM *pcm, const uint32_t ms);
// Convert a number of bytes to a floating point number of seconds
float AudioPCM_seconds(const AudioPCM *pcm, size_t n_bytes);

#ifdef AO_PULSEAUDIO
#include <pulse/sample.h>
#include <pulse/channelmap.h>
#include <pulse/def.h>

#define MPL_PA_BUF_MS 100

pa_sample_spec AudioPCM_pulseaudio_spec(const AudioPCM *pcm);
pa_channel_map AudioPCM_pulseaudio_channel_map(const AudioPCM *pcm);
pa_buffer_attr AudioPCM_pulseaudio_buffer_attr(const AudioPCM *pcm);
#endif
