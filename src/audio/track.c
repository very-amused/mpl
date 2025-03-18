#include "track.h"
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <stddef.h>
#include <libavutil/samplefmt.h>
#include <libavutil/error.h>
#include <libavformat/avformat.h>
#include <libavcodec/codec.h>

int AudioBuffer_init(AudioBuffer *ab, const AudioPCM *pcm) {
	// To start, we're going to use a fixed buffer time length of 30s.
	// Later, we'll use track header info to decide how big the initial buffer allocation is.
	static const size_t TIME_LEN = 30;

	// Compute line and buffer sizes
	ab->buf_size = av_samples_get_buffer_size(
			&ab->line_size,
			pcm->n_channels,
			(pcm->sample_rate * TIME_LEN) + 1, // Add an extra sample for ring buffer alignment
			pcm->sample_fmt, 0);
	if (ab->buf_size < 0) {
		return 1;
	}

	// Allocate buffer and set r/w indices
	ab->data = av_malloc(ab->buf_size);
	if (!ab->data) {
		return 1;
	}
	ab->rd = ab->wr = 0;

	return 0;	
}

void AudioBuffer_deinit(AudioBuffer *ab) {
	av_free(ab->data);
	ab->data = NULL;
}

int AudioTrack_init(AudioTrack *t, const char *url) {
	char av_err[AV_ERROR_MAX_STRING_SIZE]; // libav* library error message buffer

	// Zero pointers to ensure AudioTrack_deinit is safe
	t->avf_ctx = NULL;
	t->avc_ctx = NULL;
	t->codec = NULL;
	t->buffer = NULL;

	// Open track source from URL
	t->url = av_strdup(url);
	if (!t->url) {
		return 1;
	}
	// Create av format demuxing context
	int status = avformat_open_input(&t->avf_ctx, t->url, NULL, NULL);
	if (status < 0) {
		av_strerror(status, av_err, sizeof(av_err));
		fprintf(stderr, "Failed to open %s: %s\n", t->url, av_err);
		return 1;
	}

	// Let libavformat choose the best audio stream for decoding
	t->stream_no = av_find_best_stream(t->avf_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &t->codec, 0);
	if (t->stream_no < 0) {
		av_strerror(t->stream_no, av_err, sizeof(av_err));
		fprintf(stderr, "Failed to select audio stream for %s: %s\n", t->url, av_err);
		return 1;
	}
	if (t->codec == NULL) {
		fprintf(stderr, "Failed to select audio decoder for %s\n", t->url);
		return 1;
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
		fprintf(stderr, "Failed to initialize playback buffer for %s\n", t->url);
		return 1;
	}

	// Initialize decoding context
	t->avc_ctx = avcodec_alloc_context3(t->codec);
	if (t->avc_ctx == NULL) {
		fprintf(stderr, "Failed to allocate decoding context for %s\n", t->url);
		return 1;
	}
	status = avcodec_open2(t->avc_ctx, t->codec, NULL);
	if (status < 0) {
		av_strerror(status, av_err, sizeof(av_err));
		fprintf(stderr, "Failed to initialize decoding context for %s: %s\n", t->url, av_err);
		return 1;
	}

	// Allocate packet + frame memory
	t->av_packet = av_packet_alloc();
	t->av_frame = av_frame_alloc();
	if (t->av_packet == NULL || t->av_frame == NULL) {
		fprintf(stderr, "Failed to allocate packet/frame memory for %s\n", t->url);
		return 1;
	}

	// Drain any start padding
	for (size_t frame_no = 0; frame_no < t->start_padding;) {
		status = avcodec_receive_frame(t->avc_ctx, t->av_frame);
		if (status == AVERROR(EAGAIN)) {
			if (status = av_read_frame(t->avf_ctx, t->av_packet), status < 0) {
				av_strerror(status, av_err, sizeof(av_err));
				fprintf(stderr, "Failed to read packet from %s: %s\n", t->url, av_err);
				break;
			}
			avcodec_send_packet(t->avc_ctx, t->av_packet);
			continue;
		} else if (status < 0) {
			av_strerror(status, av_err, sizeof(av_err));
			fprintf(stderr, "Failed to read frame from %s: %s\n", t->url, av_err);
			continue;
		}

		frame_no++;
	}
	av_packet_unref(t->av_packet);
	av_frame_unref(t->av_frame);

	return 0;
}

void AudioTrack_deinit(AudioTrack *at) {

}

double AudioTrack_seconds(AVRational time_base, int64_t value) {
	return (double)(time_base.num * value) / time_base.den;
}
