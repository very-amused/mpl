#pragma once
#include "buffer.h"

#include <libavcodec/packet.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
#include <stddef.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

// Decoding and playback state for a single track
typedef struct AudioTrack {
	// Demuxing
	AVFormatContext *avf_ctx;
	int stream_no; // Stream # to use for audio playback

	// Decoding
	AVCodecContext *avc_ctx;
	const AVCodec *codec;
	AVPacket *av_packet;
	AVFrame *av_frame;
	
	// PCM playback
	AudioPCM pcm;
	AudioBuffer *buffer;

	// Metadata
	AVRational time_base; // Base time resolution
	int64_t duration; // Duration in {time_base} units
	double duration_secs; // Duration in seconds
	size_t start_padding, end_padding; // The number of sample frames at the start and end of the track used for padding. These must be discarded for gapless playback.
} AudioTrack;

enum AudioTrack_ERR AudioTrack_init(AudioTrack *at, const char *url);
void AudioTrack_deinit(AudioTrack *at);

// Convert a scalar in time_base units into a double precision number of seconds
double AudioTrack_seconds(AVRational time_base, int64_t value);
