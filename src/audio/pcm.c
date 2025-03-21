#include "pcm.h"

#include <libavutil/samplefmt.h>
#include <stdint.h>

uint32_t AudioPCM_buffer_size(const AudioPCM *pcm, const uint32_t ms) {
	return av_samples_get_buffer_size(
			NULL,
			pcm->n_channels,
			pcm->sample_rate * (ms / 1000.0),
			pcm->sample_fmt,
			0);
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
	pa_channel_map_init_stereo(&cm);

	// FIXME FIXME FIXME

	return cm;
}

pa_buffer_attr AudioPCM_pulseaudio_buffer_attr(const AudioPCM *pcm) {
	pa_buffer_attr buf_attr = {
		.maxlength = -1,
		.tlength = AudioPCM_buffer_size(pcm, MPL_PA_BUF_MS),
		.prebuf = 0, // Disable prebuffering threshold. Stream must manually be corked to start
		.fragsize = -1,
		.minreq = -1
	};
	return buf_attr;
}
#endif
