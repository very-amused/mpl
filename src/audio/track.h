#pragma once
#include <stddef.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>
#include <libavcodec/codec.h>

// Decoding and playback state for a single track
typedef struct AudioTrack {
	// Decoding
	const AVCodec *codec;
	
	// PCM
	uint8_t n_channels; // Number of audio channels. Currently only mono and stereo are supported
	enum AVSampleFormat sample_format;
	uint32_t sample_rate;
} AudioTrack;
