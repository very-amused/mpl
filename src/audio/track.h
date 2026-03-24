#pragma once
#include "audio/seek.h"
#include "buffer.h"
#include "config/settings.h"
#include "track_meta.h"
#include "ui/event.h"

#include <libavcodec/packet.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
#include <stddef.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifdef MPL_RESAMPLE
#include <libswresample/swresample.h>
#endif

typedef struct AudioBackend AudioBackend; // break circular dependency between AudioTrack and AudioBackend

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

	// Resampling
#ifdef MPL_RESAMPLE
	bool resample; // whether we need to resample in-house
	SwrContext *swr_ctx;
	AVFrame *av_frame_swr; // Pre-resampled frame input
#endif
	
	// PCM playback
	AudioPCM src_pcm; // (pre-resample if needed) PCM format we decode
	AudioPCM buf_pcm; // (post-resample if needed) PCM format we buffer for playback
	AudioBuffer *buffer;

	// Metadata
	// NOTE: all units of sample frames are post-resample frames
	EventBody_Timecode duration_timecode; // Duration in sample frames
	size_t start_padding, end_padding; // The number of sample frames at the start and end of the track used for padding. These must be discarded for gapless playback.

	// User settings
	const Settings *settings;
} AudioTrack;


// Initialize an AudioTrack for playback with an AudioBackend
enum AudioTrack_ERR AudioTrack_init(AudioTrack *at, const char *url, AudioBackend *ab, const Settings *settings);
void AudioTrack_deinit(AudioTrack *at);

// Retrieve metadata from an AudioTrack's decoding context, storing the result in *meta
enum AudioTrack_ERR AudioTrack_get_metadata(AudioTrack *at, TrackMeta *meta);

// Buffer one packet worth of frames and set n_bytes (if not NULL) to the number of bytes buffered in doing so.
enum AudioTrack_ERR AudioTrack_buffer_packet(AudioTrack *at, size_t *n_bytes);
// Buffer track data. AudioSeek_Relative will buffer onto the end of the Track's current AudioBuffer.
enum AudioTrack_ERR AudioTrack_buffer_ms(AudioTrack *at, enum AudioSeek dir, const uint32_t ms);
