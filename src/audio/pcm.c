#include "pcm.h"

#include <libavutil/samplefmt.h>

#ifdef AO_PULSEAUDIO
#include <pulse/sample.h>

pa_sample_spec AudioPCM_pulseaudio_spec(const AudioPCM pcm) {
	pa_sample_spec ss;
	pa_sample_spec_init(&ss);

	// Set channels
	ss.channels = pcm.n_channels;

	// Set sample format
	switch (pcm.sample_fmt) {
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
	ss.rate = pcm.sample_rate;

	return ss;
}
#endif
