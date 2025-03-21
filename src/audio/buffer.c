#include "buffer.h"
#include <libavutil/mem.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

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


size_t AudioBuffer_write(AudioBuffer *ab, unsigned char *src, size_t n) {
	size_t count = 0; // # of bytes written

	int wr = atomic_load(&ab->wr);
	const int rd = atomic_load(&ab->rd);

	while (count < n && (wr+1) % ab->line_size != rd) {
		// Write chunk
		int chunk_size = wr >= rd ? ab->line_size - wr : (rd-1) - wr;
		if (chunk_size > n) {
			chunk_size = n;
		}
		memcpy(&ab->data[wr], &src[count], chunk_size);

		// Increment write idx and count
		wr++;
		wr %= ab->line_size;
		count += chunk_size;
	}

	// Store new wr index
	atomic_store(&ab->wr, wr);

	return count;
}

size_t AudioBuffer_read(AudioBuffer *ab, unsigned char *dst, size_t n) {
	size_t count = 0; // # of bytes read

	const int wr = atomic_load(&ab->wr);
	int rd = atomic_load(&ab->rd);

	while (count < n && rd != wr) {
		// Read chunk
		int chunk_size = rd < wr ? wr - rd : ab->line_size - rd;
		memcpy(&dst[count], &ab->data[rd], chunk_size);

		// Increment read idx and count
		rd++;
		rd %= 10;
		count += chunk_size;
	}

	// Store new rd index
	atomic_store(&ab->rd, rd);

	return count;
}

