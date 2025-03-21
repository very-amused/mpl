#include "track.h"
#include "../error.h"
#include "audio/buffer.h"
#include "audio/pcm.h"

#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libavutil/error.h>
#include <libavformat/avformat.h>
#include <libavcodec/codec.h>


enum AudioTrack_ERR AudioTrack_init(AudioTrack *t, const char *url) {
	char av_err[AV_ERROR_MAX_STRING_SIZE]; // libav* library error message buffer

	// Zero pointers to ensure AudioTrack_deinit is safe
	t->avf_ctx = NULL;
	t->avc_ctx = NULL;
	t->codec = NULL;
	t->buffer = NULL;

	// Create av format demuxing context
	int status = avformat_open_input(&t->avf_ctx, url, NULL, NULL);
	if (status < 0) {
		av_perror(status, av_err);
		return AudioTrack_NOT_FOUND;
	}

	// Let libavformat choose the best audio stream for decoding
	status = avformat_find_stream_info(t->avf_ctx, NULL);
	if (status < 0) {
		av_perror(status, av_err);
		return AudioTrack_NO_STREAM;
	}
	t->stream_no = av_find_best_stream(t->avf_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &t->codec, 0);
	if (t->stream_no < 0) {
		av_perror(t->stream_no, av_err);
		return AudioTrack_NO_STREAM;
	}
	if (t->codec == NULL) {
		return AudioTrack_NO_DECODER;
	}

	// Process track header
	const AVStream *stream = t->avf_ctx->streams[t->stream_no];
	const AVCodecParameters *codec_params = stream->codecpar;
	// Sampling info
	t->pcm.sample_fmt = codec_params->format;
	t->pcm.sample_rate = codec_params->sample_rate;
	t->pcm.n_channels = codec_params->ch_layout.nb_channels;
	// Timing info
	t->time_base = stream->time_base;
	t->duration = stream->duration;
	t->duration_secs = AudioTrack_seconds(t->time_base, t->duration);
	t->start_padding = codec_params->initial_padding;
	t->end_padding = codec_params->trailing_padding;

	// Allocate playback buffer
	t->buffer = malloc(sizeof(AudioBuffer));
	if (t->buffer == NULL || AudioBuffer_init(t->buffer, &t->pcm) != 0) {
		return AudioTrack_BUFFER_ERR;
	}

	// Initialize decoding context
	t->avc_ctx = avcodec_alloc_context3(t->codec);
	if (t->avc_ctx == NULL) {
		return AudioTrack_BAD_ALLOC;
	}
	status = avcodec_open2(t->avc_ctx, t->codec, NULL);
	if (status < 0) {
		av_perror(status, av_err);
		return AudioTrack_CODEC_ERR;
	}

	// Allocate packet + frame memory
	t->av_packet = av_packet_alloc();
	t->av_frame = av_frame_alloc();
	if (t->av_packet == NULL || t->av_frame == NULL) {
		return AudioTrack_BAD_ALLOC;
	}

	// Drain any padding samples at the start
	while (t->start_padding > 0) {
		status = av_read_frame(t->avf_ctx, t->av_packet); // Despite its name, av_read_frame reads packets
		if (status < 0) {
			av_perror(status, av_err);
			return AudioTrack_PACKET_ERR;
		}
		status = avcodec_send_packet(t->avc_ctx, t->av_packet);
		if (status < 0) {
			av_perror(status, av_err);
			return AudioTrack_PACKET_ERR;
		}

		do {
			status = avcodec_receive_frame(t->avc_ctx, t->av_frame);
		} while (status >= 0);
		if (t->start_padding > 0 && status != AVERROR(EAGAIN)) {
			return AudioTrack_FRAME_ERR;
		}
	}
	while (avcodec_receive_frame(t->avc_ctx, t->av_frame) >= 0) {} // Just in case, drain any remaining frames from the packet's buffer
	av_packet_unref(t->av_packet);
	av_frame_unref(t->av_frame);

	return AudioTrack_OK;
}

void AudioTrack_deinit(AudioTrack *t) {
	av_packet_free(&t->av_packet);
	av_frame_free(&t->av_frame);

	avcodec_free_context(&t->avc_ctx);

	AudioBuffer_deinit(t->buffer);
	free(t->buffer);
	t->buffer = NULL;

	avformat_close_input(&t->avf_ctx);
}

// Buffer one packet worth of frames and set n_bytes to the number of bytes buffered in doing so.
static enum AudioTrack_ERR AudioTrack_buffer_packet(AudioTrack *t, size_t *n_bytes) {
	char av_err[AV_ERROR_MAX_STRING_SIZE]; // libav* library error message buffer
	const size_t sample_size = av_get_bytes_per_sample(t->pcm.sample_fmt);

	*n_bytes = 0;

	// Read packet
	int status;
	do {
		status = av_read_frame(t->avf_ctx, t->av_packet);
		if (status < 0) {
			if (status == AVERROR(EOF)) {
				return AudioTrack_EOF;
			}
			av_perror(status, av_err);
			return AudioTrack_PACKET_ERR;
		}
	} while (t->av_packet->stream_index != t->stream_no);

	// Decode into frames
	status = avcodec_send_packet(t->avc_ctx, t->av_packet);
	if (status < 0) {
		av_perror(status, av_err);
		return AudioTrack_PACKET_ERR;
	}

	// Buffer each frame we decode
	const bool is_planar = av_sample_fmt_is_planar(t->pcm.sample_fmt);
	status = avcodec_receive_frame(t->avc_ctx, t->av_frame);
	for (; status >= 0; status = avcodec_receive_frame(t->avc_ctx, t->av_frame)) {
		const AVFrame *frame = t->av_frame;
		if (is_planar) {
			// oh shit
		}
	}

	return AudioTrack_OK;
}

enum AudioTrack_ERR AudioTrack_buffer_ms(AudioTrack *t, enum AudioSeek dir, const uint32_t ms) {
	// Compute the number of bytes we want to buffer
	size_t n_bytes = AudioPCM_buffer_size(&t->pcm, ms);
	const size_t sample_size = av_get_bytes_per_sample(t->pcm.sample_fmt);

	// Read packets, convert them to frames, and write them to the buffer
	size_t n = 0; // # of bytes written
	while (n < sample_size) {
	}

	return AudioTrack_OK;
}

double AudioTrack_seconds(AVRational time_base, int64_t value) {
	return (double)(time_base.num * value) / time_base.den;
}

