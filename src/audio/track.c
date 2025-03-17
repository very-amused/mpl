#include "track.h"
#include <libavcodec/codec_par.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
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
	if (status != 0) {
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

	// TODO: Process track header
	const AVStream *stream = t->avf_ctx->streams[t->stream_no];
	double duration = (double)(stream->duration * stream->time_base.num) / stream->time_base.den;

	// Allocate playback buffer
	t->buffer = malloc(sizeof(AudioBuffer));
	if (t->buffer == NULL || AudioBuffer_init(t->buffer, &t->pcm) != 0) {
		fprintf(stderr, "Failed to initialize playback buffer for %s\n", t->url);
		return 1;
	}

	return 0;
}

void AudioTrack_deinit(AudioTrack *at) {

}
