#include <assert.h>
#include <ctype.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
#include <semaphore.h>
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
#include <libavutil/dict.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef MPL_RESAMPLE
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
#endif


#include "track.h"
#include "../error.h"
#include "audio/buffer.h"
#include "audio/pcm.h"
#include "audio/out/backend.h"
#include "config/settings.h"
#include "util/rational.h"
#include "util/log.h"
#include "util/compat/string_win32.h"


enum AudioTrack_ERR AudioTrack_init(AudioTrack *t, const char *url, AudioBackend *ab, const Settings *settings) {
	char av_err[AV_ERROR_MAX_STRING_SIZE]; // libav* library error message buffer

	// Zero pointers to ensure AudioTrack_deinit is safe
	memset(t, 0, sizeof(AudioTrack));
	t->settings = settings;

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
	t->src_pcm.sample_fmt = codec_params->format;
	t->src_pcm.sample_rate = codec_params->sample_rate;
	t->src_pcm.n_channels = codec_params->ch_layout.nb_channels;
	LOG(Verbosity_VERBOSE, "AudioTrack: is_planar: %d\n\tsample rate: %d\nsample_fmt: %s\n",
			av_sample_fmt_is_planar(t->src_pcm.sample_fmt), t->src_pcm.sample_rate, av_get_sample_fmt_name(t->src_pcm.sample_fmt));

	// Negotiate in-house resampling iff the AudioBackend is incapable of its own resampling
	// We do this early so t->buf_pcm is set correctly for timing info and buffering
#ifdef MPL_RESAMPLE
	t->resample = AudioBackend_negotiate_pcm(ab, &t->buf_pcm, &t->src_pcm);
#else
	t->buf_pcm = t->src_pcm;
#endif

	// Compute timing info
	const AVRational time_base = stream->time_base;
	const int64_t duration_tb = stream->duration; // Duration in time_base units
	mplRational duration;
	mplRational_from_AVRational(&duration, time_base);
	duration.num *= duration_tb;
	duration.num *= t->buf_pcm.sample_rate;
	mplRational_reduce(&duration);
	if (duration.den == 1) {
		t->duration_timecode = duration.num;
	} else {
		LOG(Verbosity_VERBOSE, "Warning: Unable to compute duration timecode without loss of precision.\n");
		t->duration_timecode = mplRational_d(&duration);
	}
	t->start_padding = codec_params->initial_padding;
	t->end_padding = codec_params->trailing_padding;

	// Allocate playback buffer
	t->buffer = malloc(sizeof(AudioBuffer));
	if (t->buffer == NULL || AudioBuffer_init(t->buffer, &t->buf_pcm, t->settings) != 0) {
		return AudioTrack_BUFFER_ERR;
	}

	// Initialize decoding context
	t->avc_ctx = avcodec_alloc_context3(t->codec);
	if (t->avc_ctx == NULL) {
		return AudioTrack_BAD_ALLOC;
	}
	// Set decoder params from container metadata
	status = avcodec_parameters_to_context(t->avc_ctx, stream->codecpar);
	if (status < 0) {
		av_perror(status, av_err);
		return AudioTrack_CODEC_ERR;
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

	// Initialize resampling if needed
#ifdef MPL_RESAMPLE
	if (t->resample) {
		// Initialize swr resampling context
		status = swr_alloc_set_opts2(&t->swr_ctx,
				&codec_params->ch_layout, t->buf_pcm.sample_fmt, t->buf_pcm.sample_rate,
				&codec_params->ch_layout, t->src_pcm.sample_fmt, t->src_pcm.sample_rate,
				0, NULL);
		if (status < 0) {
			av_perror(status, av_err);
			return AudioTrack_RESAMPLE_ERR;
		}
		status = swr_init(t->swr_ctx);
		if (status < 0) {
			av_perror(status, av_err);
			return AudioTrack_RESAMPLE_ERR;
		}
		// Initialize SWR input frame struct
		t->av_frame_swr = av_frame_alloc();
		if (t->av_frame_swr == NULL) {
			return AudioTrack_BAD_ALLOC;
		}
	}
#endif

	return AudioTrack_OK;
}

void AudioTrack_deinit(AudioTrack *t) {
	av_packet_free(&t->av_packet);
	av_frame_free(&t->av_frame);
#ifdef MPL_RESAMPLE
	if (t->resample) {
		swr_free(&t->swr_ctx);
		av_frame_free(&t->av_frame_swr);
	}
#endif

	avcodec_free_context(&t->avc_ctx);

	AudioBuffer_deinit(t->buffer);
	free(t->buffer);
	t->buffer = NULL;

	avformat_close_input(&t->avf_ctx);
}

enum AudioTrack_ERR AudioTrack_get_metadata(AudioTrack *at, TrackMeta *meta) {
	const AVDictionary *av_metadata = at->avf_ctx->metadata;
	// Lowercase metadata key string
	static const size_t LKEY_MAX = 255;
	char lkey[LKEY_MAX+1];
	size_t lkey_len = 0;

	uint32_t tagno = 0;
	for (const AVDictionaryEntry *tag = av_dict_iterate(av_metadata, NULL); tag != NULL; tag = av_dict_iterate(av_metadata, tag), tagno++) {
		// Normalize key to lowercase
		lkey_len = strlen(tag->key);
		if (lkey_len > LKEY_MAX) {
			fprintf(stderr, "Error: metadata key exceeds max length of %zu, skipping (%s)\n", LKEY_MAX, tag->key);
			continue;
		}
		for (size_t i = 0; i < lkey_len; i++) {
			lkey[i] = tolower(tag->key[i]);
		}
		lkey[lkey_len] = '\0';

		switch (lkey[0]) {
		case 'a':
		{
			if (strcmp(lkey, "artist") == 0) {
				meta->artist_len = strlen(tag->value);
				meta->artist = strndup(tag->value, meta->artist_len);
			} else if (strcmp(lkey, "album") == 0) {
				meta->album_len = strlen(tag->value);
				meta->album = strndup(tag->value, meta->album_len);
			}
			break;
		}

		case 't':
		{
			if (strcmp(lkey, "title") == 0) {
				meta->name_len = strlen(tag->value);
				meta->name = strndup(tag->value, meta->name_len);
			} else if (strcmp(lkey, "track") == 0) {
				// TODO: parse trackno
			}
			break;
		}
		}
		LOG(Verbosity_DEBUG, "tag %d: %s=%s\n", tagno, tag->key, tag->value);
	}

	return AudioTrack_OK;
}

// Advance t->av_frame to the next buffer-ready (resampled if-needed) AVFrame.
// Called inside of AudioTrack_buffer_packet
//
// Return value is an averror
static int AudioTrack_advance_frame(AudioTrack *t) {
#ifdef MPL_RESAMPLE
	if (t->resample) {
		// Ensure resample target is non-planar to avoid UB
		if (av_sample_fmt_is_planar(t->buf_pcm.sample_fmt)) {
			LOG(Verbosity_NORMAL, "We can't resample to a planar format! One of the AudioPCM_from_* methods messed up.\n");
			return -1;
		}

		// Pump swr_ctx with input frames until either
		// 1. we have a resampled output frame with 1+ samples per-ch (thus, t->av_frame has been advanced) or
		// 2. the packet runs out of input frames (thus, the caller must call AudioTrack_buffer_packet again)
		int status;
		while (status = avcodec_receive_frame(t->avc_ctx, t->av_frame_swr), status >= 0) {
			// Get the max # of outsamples this frame will yield
			const int frame_insamples = t->av_frame_swr->nb_samples; // per ch
			const int frame_outsamples_max = swr_get_out_samples(t->swr_ctx, frame_insamples); // per ch

			// Perform resampling, using t->av_frame->extended as our dst for 1+ frames
			if (frame_outsamples_max) {
				av_frame_unref(t->av_frame);
				t->av_frame->sample_rate = t->buf_pcm.sample_rate;
				t->av_frame->format = t->buf_pcm.sample_fmt;
				t->av_frame->nb_samples = frame_outsamples_max;
				av_channel_layout_default(&t->av_frame->ch_layout, t->buf_pcm.n_channels);
				av_frame_get_buffer(t->av_frame, 0);
			}
			t->av_frame->nb_samples = swr_convert(t->swr_ctx,
					&t->av_frame->data[0], frame_outsamples_max,
					(const uint8_t **)t->av_frame_swr->extended_data, frame_insamples);
			if (t->av_frame->nb_samples) {
				return 0;
			}
		}
		return status;
	}
#endif
	return avcodec_receive_frame(t->avc_ctx, t->av_frame);
}

enum AudioTrack_ERR AudioTrack_buffer_packet(AudioTrack *t, size_t *n_bytes) {
	char av_err[AV_ERROR_MAX_STRING_SIZE]; // libav* library error message buffer

	if (n_bytes) {
		*n_bytes = 0;
	}

	// Read packet
	int status;
	do {
		// WARNING: av_read_frame does NOT unref buffers
		av_packet_unref(t->av_packet);
		status = av_read_frame(t->avf_ctx, t->av_packet);
		if (status < 0) {
			av_packet_unref(t->av_packet);
			if (status == AVERROR_EOF) {
				return AudioTrack_EOF;
			}
			av_perror(status, av_err);
			return AudioTrack_PACKET_ERR;
		}
	} while (t->av_packet->stream_index != t->stream_no);

	// Decode into frames
	status = avcodec_send_packet(t->avc_ctx, t->av_packet);
	if (status != 0) {
		av_packet_unref(t->av_packet);
		av_perror(status, av_err);
		return AudioTrack_PACKET_ERR;
	}

	unsigned char *interleave_buf = NULL; // Intermediate buffer used to interleave planar samples before they're written to the playback buffer
	unsigned int interleave_buf_size = 0;

	// Buffer each frame we decode
	const bool is_planar = av_sample_fmt_is_planar(t->buf_pcm.sample_fmt);
	const size_t buf_sample_size = av_get_bytes_per_sample(t->buf_pcm.sample_fmt);
	status = AudioTrack_advance_frame(t);
	for (; status >= 0; status = AudioTrack_advance_frame(t)) {
		const AVFrame *frame = t->av_frame;
		size_t frame_size = frame->nb_samples * t->buf_pcm.n_channels * buf_sample_size;
		LOG(Verbosity_DEBUG, "frame_size=%zu\n", frame_size);

		unsigned char *frame_data = NULL;
		if (is_planar) {
			// Interleave samples
			av_fast_malloc(&interleave_buf, &interleave_buf_size, frame_size);

			size_t interleave_idx = 0;
			for (size_t samp = 0; samp < frame->nb_samples; samp++) {
				for (size_t ch = 0; ch < t->buf_pcm.n_channels; ch++) {
					static const size_t n_data = sizeof(frame->data) / sizeof(frame->data[0]);
					unsigned char *line = ch < n_data ? frame->data[ch] : frame->extended_data[ch];
					unsigned char *sample = &line[samp * buf_sample_size];
					memcpy(&interleave_buf[interleave_idx], sample, buf_sample_size);
					interleave_idx += buf_sample_size;
				}
			}

			// Buffer interleaved result
			frame_data = interleave_buf;
		} else {
			// Our result is already interleaved
			frame_data = frame->data[0];
		}

		size_t n = 0;
		while (n < frame_size) {
			n += AudioBuffer_write(t->buffer, &frame_data[n], frame_size - n);
			if (n < frame_size) {
				sem_wait(&t->buffer->rd_sem);
			}
		}
		if (n_bytes) {
			*n_bytes += n;
		}
	}
	av_freep(&interleave_buf);
	av_frame_unref(t->av_frame);
	av_packet_unref(t->av_packet);

	return AudioTrack_OK;
}

enum AudioTrack_ERR AudioTrack_buffer_ms(AudioTrack *t, enum AudioSeek dir, const uint32_t ms) {
	// Compute the number of bytes we want to buffer
	const size_t n_bytes = AudioPCM_buffer_size(&t->buf_pcm, ms);

	// Read packets, convert them to frames, and write them to the buffer
	size_t n = 0; // # of bytes written
	while (n < n_bytes) {
		size_t packet_n;
		enum AudioTrack_ERR err = AudioTrack_buffer_packet(t, &packet_n);
		if (err != AudioTrack_OK) {
			return err;
		}
		n += packet_n;
	}

	return AudioTrack_OK;
}
