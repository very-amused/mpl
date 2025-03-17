#include "track.h"
#include <libavutil/mem.h>
#include <stddef.h>
#include <libavutil/samplefmt.h>

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
