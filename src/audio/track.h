#pragma once
#include "audio/seek.h"
#include "buffer.h"
#include "ui/event.h"

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
	RingBuffer *buffer;

	// Metadata
	AVRational time_base; // Base time resolution
	int64_t duration; // Duration in {time_base} units
	EventBody_Timecode duration_timecode; // Duration in sample frames
	size_t start_padding, end_padding; // The number of sample frames at the start and end of the track used for padding. These must be discarded for gapless playback.
} AudioTrack;

enum AudioTrack_ERR AudioTrack_init(AudioTrack *at, const char *url);
void AudioTrack_deinit(AudioTrack *at);

// Buffer one packet worth of frames and set n_bytes (if not NULL) to the number of bytes buffered in doing so.
enum AudioTrack_ERR AudioTrack_buffer_packet(AudioTrack *at, size_t *n_bytes);
// Buffer track data. AudioSeek_Relative will buffer onto the end of the Track's current AudioBuffer.
enum AudioTrack_ERR AudioTrack_buffer_ms(AudioTrack *at, enum AudioSeek dir, const uint32_t ms);
