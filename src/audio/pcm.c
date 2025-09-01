#include "pcm.h"

#include <libavutil/samplefmt.h>
#include <stdint.h>
#include <stdio.h>

uint32_t AudioPCM_buffer_size(const AudioPCM *pcm, const uint32_t ms) {
	return av_samples_get_buffer_size(
			NULL,
			pcm->n_channels,
			pcm->sample_rate * (ms / 1000.0),
			pcm->sample_fmt,
			0);
}

float AudioPCM_seconds(const AudioPCM *pcm, size_t n_bytes) {
	const size_t line_size = pcm->n_channels * av_get_bytes_per_sample(pcm->sample_fmt);
	const size_t byte_rate = line_size * pcm->sample_rate;
	return (float)n_bytes / byte_rate;
}

#ifdef AO_PULSEAUDIO
#include <pulse/sample.h>
#include <pulse/channelmap.h>
#include <pulse/def.h>

pa_sample_spec AudioPCM_pulseaudio_spec(const AudioPCM *pcm) {
	pa_sample_spec ss;
	pa_sample_spec_init(&ss);

	// Set channels
	ss.channels = pcm->n_channels;

	// Set sample format
	switch (pcm->sample_fmt) {
		case AV_SAMPLE_FMT_U8:
		case AV_SAMPLE_FMT_U8P:
			ss.format = PA_SAMPLE_U8;
			break;
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			ss.format = PA_SAMPLE_S16NE;
			break;
		case AV_SAMPLE_FMT_S32:
		case AV_SAMPLE_FMT_S32P:
			ss.format = PA_SAMPLE_S32NE;
			break;
		case AV_SAMPLE_FMT_FLT:
		case AV_SAMPLE_FMT_FLTP:
			ss.format = PA_SAMPLE_FLOAT32NE;
			break;
		default:
			ss.format = PA_SAMPLE_INVALID;
			break; // Other formats are supported by libav but not pulseaudio
	}

	// Set sample rate
	ss.rate = pcm->sample_rate;

	return ss;
}

pa_channel_map AudioPCM_pulseaudio_channel_map(const AudioPCM *pcm) {
	pa_channel_map cm;

	// TODO: support >2 channels
	switch (pcm->n_channels) {
	case 2:
		pa_channel_map_init_stereo(&cm);
		break;
	case 1:
		pa_channel_map_init_mono(&cm);
		break;
	default:
		pa_channel_map_init(&cm);
	}

	return cm;
}

pa_buffer_attr AudioPCM_pulseaudio_buffer_attr(const AudioPCM *pcm, uint32_t ab_buffer_ms) {
	pa_buffer_attr buf_attr = {
		.maxlength = -1,
		.tlength = AudioPCM_buffer_size(pcm, ab_buffer_ms),
		.prebuf = 0, // Disable prebuffering threshold. Stream must manually be corked to start
		.fragsize = -1,
		.minreq = -1
	};
	return buf_attr;
}
#endif

#ifdef AO_PIPEWIRE

struct spa_audio_info_raw AudioPCM_pipewire_info(const AudioPCM *pcm) {
	struct spa_audio_info_raw info;

	// Set channels + channel map
	info.channels = pcm->n_channels;
	// FIXME FIXME FIXME
	info.position[0] = SPA_AUDIO_CHANNEL_FL;
	info.position[1] = SPA_AUDIO_CHANNEL_FR;
	for (size_t i = 2; i < info.channels; i++)  {
		info.position[i] = SPA_AUDIO_CHANNEL_UNKNOWN;
	}

	// Set sample format
	switch (pcm->sample_fmt) {
		case AV_SAMPLE_FMT_U8:
		case AV_SAMPLE_FMT_U8P:
			info.format = SPA_AUDIO_FORMAT_U8;
			break;
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			info.format = SPA_AUDIO_FORMAT_S16;
			break;
		case AV_SAMPLE_FMT_S32:
		case AV_SAMPLE_FMT_S32P:
			info.format = SPA_AUDIO_FORMAT_S32;
			break;
		case AV_SAMPLE_FMT_FLT:
		case AV_SAMPLE_FMT_FLTP:
			info.format = SPA_AUDIO_FORMAT_F32;
			break;
		case AV_SAMPLE_FMT_DBL:
		case AV_SAMPLE_FMT_DBLP:
			info.format = SPA_AUDIO_FORMAT_F64;
			break;
		default:
			info.format = SPA_AUDIO_FORMAT_UNKNOWN;
			break; // Other formats are supported by libav but not pipewire
	}

	// Set sample rate
	info.rate = pcm->sample_rate;

	return info;
}
#endif
