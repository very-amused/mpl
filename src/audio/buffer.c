#include "buffer.h"
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <semaphore.h>

int AudioBuffer_init(AudioBuffer *buf, const AudioPCM *pcm) {
	// To start, we're going to use a fixed buffer time length of 30s.
	// Later, we'll use track header info to decide how big the initial buffer allocation is.
	static const size_t TIME_LEN = 30;

	// Compute line and buffer sizes
	buf->size = av_samples_get_buffer_size(
			NULL,
			pcm->n_channels,
			(pcm->sample_rate * TIME_LEN) + 1, // Add an extra sample for ring buffer alignment
			pcm->sample_fmt, 0);
	if (buf->size < 0) {
		return 1;
	}

	// Allocate buffer and set r/w indices
	// NOTE: we zero this so an accidental read of unintialized data is silent,
	// instead of loud and often surprising white noise
	buf->data = av_mallocz(buf->size);
	if (!buf->data) {
		return 1;
	}
	buf->rd = 0;
	buf->wr = 0;

	// Initialize semaphores
	sem_init(&buf->rd_sem, 0, 1);
	sem_init(&buf->wr_sem, 0, 1);

	return 0;
}

void AudioBuffer_deinit(AudioBuffer *buf) {
	av_free(buf->data);
	buf->data = NULL;
}


size_t AudioBuffer_write(AudioBuffer *buf, unsigned char *src, size_t n) {
	size_t count = 0; // # of bytes written

	int wr = atomic_load(&buf->wr);
	const int rd = atomic_load(&buf->rd);

	while (count < n && (wr+1) % buf->size != rd) {
		// Write chunk
		int chunk_size = wr >= rd ? buf->size - wr : (rd-1) - wr;
		if (chunk_size > n) {
			chunk_size = n;
		}
		if (chunk_size > n-count) {
			chunk_size = n - count;
		}
		memcpy(&buf->data[wr], &src[count], chunk_size);

		// Increment write idx and count
		wr += chunk_size;
		wr %= buf->size;
		count += chunk_size;
	}

	// Store new wr index
	atomic_store(&buf->wr, wr);
	// Notify any parties waiting on a write
	sem_post(&buf->wr_sem);

	return count;
}

size_t AudioBuffer_read(AudioBuffer *buf, unsigned char *dst, size_t n) {
	size_t count = 0; // # of bytes read

	const int wr = atomic_load(&buf->wr);
	int rd = atomic_load(&buf->rd);

	while (count < n && rd != wr) {
		// Read chunk
		int chunk_size = rd < wr ? wr - rd : buf->size - rd;
		if (chunk_size > n) {
			chunk_size = n;
		}
		if (chunk_size > n - count) {
			chunk_size = n - count;
		}
		memcpy(&dst[count], &buf->data[rd], chunk_size);

		// Increment read idx and count
		rd += chunk_size;
		rd %= buf->size;
		count += chunk_size;
	}

	// Store new rd index
	atomic_store(&buf->rd, rd);
	// Notify any parties waiting on a read
	sem_post(&buf->rd_sem);

	return count;
}

