#include "pcm.h"
#include "util/log.h"

#include <libavutil/samplefmt.h>
#include <stdint.h>
#include <stdio.h>
#include <winnt.h>

uint32_t AudioPCM_sample_size(const AudioPCM *pcm) {
	return av_get_bytes_per_sample(pcm->sample_fmt);
}

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

#ifdef AO_WASAPI
#include <windows.h>
#include <initguid.h>
#include <ksmedia.h>
#include <mmreg.h>
#include <mmeapi.h>
#include <minwindef.h>
#include <string.h>

WAVEFORMATEXTENSIBLE AudioPCM_wasapi_waveformat(const AudioPCM *pcm) {
	// Get sample format and size
	GUID subformat;
	WORD sample_size;
	switch (pcm->sample_fmt) {
		case AV_SAMPLE_FMT_U8:
		case AV_SAMPLE_FMT_U8P:
			subformat = KSDATAFORMAT_SUBTYPE_PCM;
			sample_size = 8;
			break;
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			subformat = KSDATAFORMAT_SUBTYPE_PCM;
			sample_size = 16;
			break;
		case AV_SAMPLE_FMT_S32:
		case AV_SAMPLE_FMT_S32P:
			subformat = KSDATAFORMAT_SUBTYPE_PCM;
			sample_size = 32;
			break;
		case AV_SAMPLE_FMT_S64:
		case AV_SAMPLE_FMT_S64P:
			subformat = KSDATAFORMAT_SUBTYPE_PCM;
			sample_size = 64;
			break;
		case AV_SAMPLE_FMT_FLT:
		case AV_SAMPLE_FMT_FLTP:
			subformat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
			sample_size = 32;
			break;
		case AV_SAMPLE_FMT_DBL:
		case AV_SAMPLE_FMT_DBLP:
			subformat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
			sample_size = 64;
			break;
		default:
		{
			WAVEFORMATEXTENSIBLE fmt;
			memset(&fmt, 0, sizeof(fmt));
			fmt.Format.wFormatTag = WAVE_FORMAT_UNKNOWN;
			return fmt;
		}
	}

	// Basic format info
	const WAVEFORMATEX basic_format = {
		.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
		.nChannels = pcm->n_channels,
		.nSamplesPerSec = pcm->sample_rate,
		.nAvgBytesPerSec = AudioPCM_buffer_size(pcm, 1000),
		.nBlockAlign = (pcm->n_channels * sample_size) / 8,
		.wBitsPerSample = sample_size,
		.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
	};

	// Extended format info (enables support for >2 channels)
	WAVEFORMATEXTENSIBLE fmt = {0};
	memset(&fmt, 0, sizeof(fmt));
	fmt.Format = basic_format;
	fmt.Samples.wValidBitsPerSample = basic_format.wBitsPerSample;
	// Channel layout
	// TODO: support >2 channels
	switch (pcm->n_channels) {
	case 1:
		fmt.dwChannelMask = KSAUDIO_SPEAKER_MONO;
		break;
	case 2:
		fmt.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
		break;
	}
	// Subformat (either signed or fp PCM)
	fmt.SubFormat = subformat;

	return fmt;
}

int AudioPCM_from_wasapi_waveformat(AudioPCM *dst_pcm, const WAVEFORMATEX *wavfmt) {
	dst_pcm->n_channels = wavfmt->nChannels; 
	dst_pcm->sample_rate = wavfmt->nSamplesPerSec;

	switch (wavfmt->wFormatTag) {
	case WAVE_FORMAT_EXTENSIBLE:
	{
		const WAVEFORMATEXTENSIBLE *wavfmt_ex = (const WAVEFORMATEXTENSIBLE *)wavfmt;
		if (IsEqualGUID(&wavfmt_ex->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)) {
			goto wasapi_pcm_fmt;
		} else if (IsEqualGUID(&wavfmt_ex->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
			goto wasapi_float_fmt;
		}
		return 1;
	}
	break;

	case WAVE_FORMAT_PCM:
wasapi_pcm_fmt:
		switch (wavfmt->wBitsPerSample) {
		case 8:
			dst_pcm->sample_fmt = AV_SAMPLE_FMT_U8;
			break;
		case 16:
			dst_pcm->sample_fmt = AV_SAMPLE_FMT_S16;
			break;
		case 32:
			dst_pcm->sample_fmt = AV_SAMPLE_FMT_S32;
			break;
		case 64:
			dst_pcm->sample_fmt = AV_SAMPLE_FMT_S64;
			break;
		default:
			return 1;
		}
	break;
	
	case WAVE_FORMAT_IEEE_FLOAT:
wasapi_float_fmt:
		switch (wavfmt->wBitsPerSample) {
		case 32:
			dst_pcm->sample_fmt = AV_SAMPLE_FMT_FLT;
			break;
		case 64:
			dst_pcm->sample_fmt = AV_SAMPLE_FMT_DBL;
			break;
		default:
			return 1;
		}
	break;
	
	default:
		return 1;
	}

	return 0;
}

#ifdef MPL_DEBUG
#include <assert.h>

void AudioPCM_wasapi_debug(const AudioPCM *pcm, const WAVEFORMATEXTENSIBLE *fmt) {
	assert(fmt->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE);
	assert(fmt->Format.nChannels == 2);
	assert(fmt->Format.nSamplesPerSec == 44100);
	assert(fmt->Format.nAvgBytesPerSec == AudioPCM_buffer_size(pcm, 1000));
	assert(fmt->Format.nBlockAlign == (fmt->Format.nChannels * fmt->Format.wBitsPerSample) / 8);
	assert(fmt->Format.wBitsPerSample == 16);
	assert(fmt->Format.cbSize == 22);

	assert(fmt->Samples.wValidBitsPerSample == fmt->Format.wBitsPerSample);
	assert(fmt->Samples.wReserved == 0);
}
#endif
#endif
