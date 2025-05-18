#include <ctype.h>
#include <errno.h>
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
#include <stdlib.h>
#include <string.h>

#include "track.h"
#include "../error.h"
#include "audio/buffer.h"
#include "audio/pcm.h"
#include "util/rational.h"
#include "util/log.h"


enum AudioTrack_ERR AudioTrack_init(AudioTrack *t, const char *url) {
	char av_err[AV_ERROR_MAX_STRING_SIZE]; // libav* library error message buffer

	// Zero pointers to ensure AudioTrack_deinit is safe
	memset(t, 0, sizeof(AudioTrack));

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
	LOG(Verbosity_DEBUG, "AudioTrack: is_planar: %d\n\tsample rate: %d\nsample_fmt: %s\n", av_sample_fmt_is_planar(t->pcm.sample_fmt), t->pcm.sample_rate, av_get_sample_fmt_name(t->pcm.sample_fmt));
	// Timing info
	const AVRational time_base = stream->time_base;
	const int64_t duration_tb = stream->duration; // Duration in time_base units
	mplRational duration;
	mplRational_from_AVRational(&duration, time_base);
	duration.num *= duration_tb;
	duration.num *= t->pcm.sample_rate;
	mplRational_reduce(&duration);
	if (duration.den == 1) {
		t->duration_timecode = duration.num;
	} else {
		fprintf(stderr, "Warning: Unable to compute duration timecode without loss of precision.\n");
		t->duration_timecode = mplRational_d(&duration);
	}
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
	LOG(Verbosity_DEBUG, "Track start_padding: %zu\n", t->start_padding);
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

enum AudioTrack_ERR AudioTrack_buffer_packet(AudioTrack *t, size_t *n_bytes) {
	char av_err[AV_ERROR_MAX_STRING_SIZE]; // libav* library error message buffer
	const size_t sample_size = av_get_bytes_per_sample(t->pcm.sample_fmt);

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

	const bool is_planar = av_sample_fmt_is_planar(t->pcm.sample_fmt);
	unsigned char *interleave_buf = NULL; // Intermediate buffer used to interleave planar samples before they're written to the playback buffer
	unsigned int interleave_buf_size = 0;

	// Buffer each frame we decode
	status = avcodec_receive_frame(t->avc_ctx, t->av_frame);
	for (; status >= 0; status = avcodec_receive_frame(t->avc_ctx, t->av_frame)) {
		const AVFrame *frame = t->av_frame;
		size_t frame_size = frame->nb_samples * t->pcm.n_channels * sample_size;

		unsigned char *frame_data = NULL;
		if (is_planar) {
			// Interleave samples
			av_fast_malloc(&interleave_buf, &interleave_buf_size, frame_size);

			size_t interleave_idx = 0;
			for (size_t samp = 0; samp < frame->nb_samples; samp++) {
				for (size_t ch = 0; ch < t->pcm.n_channels; ch++) {
					static const size_t n_data = sizeof(frame->data) / sizeof(frame->data[0]);
					unsigned char *line = ch < n_data ? frame->data[ch] : frame->extended_data[ch];
					unsigned char *sample = &line[samp * sample_size];
					memcpy(&interleave_buf[interleave_idx], sample, sample_size);
					interleave_idx += sample_size;
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
	const size_t n_bytes = AudioPCM_buffer_size(&t->pcm, ms);

	// Read packets, convert them to frames, and write them to the buffer
	size_t n = 0; // # of bytes written
	while (n < n_bytes) {
		size_t packet_n;
		enum AudioTrack_ERR err = AudioTrack_buffer_packet(t, &packet_n);
		if (err == AudioTrack_EOF) {
			return err;
		} else if (err != AudioTrack_OK) {
			return err;
		}
		n += packet_n;
	}

	return AudioTrack_OK;
}
