#pragma once
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
#include <stddef.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

// Sampling parameters used to interpret decoded PCM frames
typedef struct AudioPCM {
	enum AVSampleFormat sample_fmt;
	uint32_t sample_rate;
	uint8_t n_channels;
} AudioPCM;

// A ring buffer used to hold decoded PCM samples
typedef struct AudioBuffer {
	// The raw byte size of the buffer.
	ssize_t buf_size;
	// The 'line size' of the buffer. if is_planar is true,
	// this is the size of one channel's worth of consecutive samples.
	// Otherwise, this is equal to buf_size and all channels' worth of interleaved samples.
	// NOTE: The buffer wraps mod line_size, NOT mod buf_size.
	int line_size;

	unsigned char *data;
	int rd, wr; // Read/write indices relative to line_size
} AudioBuffer;

// Initialize an AudioBuffer for use
int AudioBuffer_init(AudioBuffer *ab, const AudioPCM *pcm);
// Deinitialize an AudioBuffer for freeing
void AudioBuffer_deinit(AudioBuffer *ab);

// Decoding and playback state for a single track
typedef struct AudioTrack {
	// Demuxing
	char *url; // Track URL passed to libavformat. Null terminated
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

int AudioTrack_init(AudioTrack *at, const char *url);
void AudioTrack_deinit(AudioTrack *at);

// Convert a scalar in time_base units into a double precision number of seconds
double AudioTrack_seconds(AVRational time_base, int64_t value);
