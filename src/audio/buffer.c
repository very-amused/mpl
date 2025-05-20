#include <limits.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <semaphore.h>

#include "buffer.h"
#include "audio/seek.h"
#include "error.h"
#include "util/log.h"

int AudioBuffer_init(AudioBuffer *buf, const AudioPCM *pcm) {
	// To start, we're going to use a fixed buffer time length of 30s.
	// Later, we'll use track header info to decide how big the initial buffer allocation is.
	static const size_t TIME_LEN = 60;

	// Compute line and buffer sizes
	buf->size = av_samples_get_buffer_size(
			NULL,
			pcm->n_channels,
			(pcm->sample_rate * TIME_LEN) + 1, // Add an extra sample for ring buffer alignment
			pcm->sample_fmt, 0);
	buf->frame_size = av_get_bytes_per_sample(pcm->sample_fmt) * pcm->n_channels;
	//buf->size = buf->frame_size * ((pcm->sample_rate * TIME_LEN) + 1);

	// Allocate buffer and set r/w indices
	// NOTE: we zero this so an accidental read of unintialized data is silent,
	// instead of loud and often surprising white noise
	buf->data = av_mallocz(buf->size);
	if (!buf->data) {
		return 1;
	}
	buf->rd = 0;
	buf->wr = 0;
	buf->n_read = 0;
	buf->n_written = 0;

	// Initialize semaphores
	sem_init(&buf->rd_sem, 0, 0);
	sem_init(&buf->wr_sem, 0, 0);

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
	// Increment cumulative bytes written
	buf->n_written += count;

	return count;
}


size_t AudioBuffer_read(AudioBuffer *buf, unsigned char *dst, size_t n, bool align) {
	size_t count = 0; // # of bytes read

	const int wr = atomic_load(&buf->wr);
	int rd = atomic_load(&buf->rd);

	if (align) {
		const size_t max_read = AudioBuffer_max_read(buf, rd, wr, align);
		if (max_read < n) {
			n = max_read;
		}
	}

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
	// Increment cumulative bytes read
	buf->n_read += count;

	return count;
}

const size_t AudioBuffer_max_read(const AudioBuffer *buf, int rd, int wr, bool align) {
	// Load rd/wr directly if not provided
	if (rd < 0 || wr < 0) {
		rd = atomic_load(&buf->rd);
		wr = atomic_load(&buf->wr);
	}

	size_t maxrd;
	if (wr >= rd) {
		maxrd = wr - rd;
	} else {
		maxrd = buf->size - (rd - wr);
	}

	if (align) {
		maxrd -= (maxrd % buf->frame_size);
	}
	
	return maxrd;
}

// Try to seek n bytes backwards in an AudioBuffer
static int AudioBuffer_seek_backward(AudioBuffer *buf, int n, enum AudioSeek from,
		int rd, const int wr);
// Try to seek n bytes forwards in an AudioBuffer
static int AudioBuffer_seek_forward(AudioBuffer *buf, int n, enum AudioSeek from,
		int rd, const int wr);

// WIP
int AudioBuffer_seek(AudioBuffer *buf, int64_t offset_bytes, enum AudioSeek from) {

	// For now, support only relative seeks
	if (from != AudioSeek_Relative) {
		fprintf(stderr, "Only relative seeks are currently supported\n");
		return 1;
	}

	// Check if our seek is within valid data bounds
	if (offset_bytes == 0) {
		// Handle a zero seek as a noop
		return 0;
	}
	// TODO: how likely are we to actually exceed INT_MAX
	if (offset_bytes > INT_MAX || offset_bytes < INT_MIN) {
		return -1;
	}

	const int rd = atomic_load(&buf->rd);
	const int wr = atomic_load(&buf->wr);
	if (rd == wr) {
		// Can't seek in an empty buffer
		return -1;
	}

	if (offset_bytes < 0) {
		int offset_scalar = -offset_bytes;
		return AudioBuffer_seek_backward(buf, offset_scalar, from, rd, wr);
	} else {
		return AudioBuffer_seek_forward(buf, offset_bytes, from, rd, wr);
		/*
		// Check if offset is within valid data
		const int64_t ahead_max = rd <= wr ? wr-rd : buf->size - (wr-rd);
		if (offset_bytes > ahead_max) {
			return -1;
		}

		// Perform the seek
		if (rd+offset_bytes <= wr) {
			LOG(Verbosity_VERBOSE, "Seek type 2\n");
			atomic_fetch_add(&buf->rd, offset_bytes); // type 1
		} else {
			LOG(Verbosity_VERBOSE, "Seek type 3\n");
			atomic_store(&buf->rd, offset_bytes - (buf->size - rd)); // type 2
		}
		buf->n_read += offset_bytes;
		return 0;
		*/
	}

	return 1;
}

static int AudioBuffer_seek_backward(AudioBuffer *buf, int n, enum AudioSeek from,
		int rd, int wr) {
	// Sanity check n
	if (n >= buf->size || n < 0) {
		return -1;
	}

	switch (from) {
	case AudioSeek_Relative:
	{
		// Whether our seek can wrap around the buffer
		const bool wrap = rd < wr && buf->n_written >= buf->size;

		// Handle non-wrapping seeks (these are easy)
		if (!wrap) {
			const int s_max = wr < rd ? (rd - (wr+1)) : rd; // inclusive seek limit
			// Don't block seeks to track start
			if (buf->n_written < buf->size && n > s_max) {
				n = s_max;
			}
			if (n > s_max) {
				return -1;
			}
			rd -= n;
			buf->n_read -= n;
			atomic_store(&buf->rd, rd);
			return 0;
		}

		// Handle wrapping seeks
		const int size = buf->size;
		const int s_max = size - ((wr+1) - rd);
		if (n > s_max) {
			return -1;
		}
		rd -= n;
		if (rd < 0) {
			// take rd %= n using arithmetic mod.
			rd = size + rd; // e.g (0 - 1 % 10 -> 9)
		}
		atomic_store(&buf->rd, rd);
		buf->n_read -= n;
		return 0;
	}

	default:
		break;
	}

	return -1;
}

static int AudioBuffer_seek_forward(AudioBuffer *buf, int n, enum AudioSeek from,
	int rd, const int wr) {
	// Sanity check n
	if (n >= buf->size || n < 0) {
		return -1;
	}

	// Whether our seek can wrap around the buffer
	const bool wrap = wr < rd;

	// Handle non-wrapping seeks
	if (!wrap) {
		const int s_max = (wr-1) - rd;
		if (n > s_max) {
			return -1;
		}
		rd += n;
		buf->n_read += n;
		atomic_store(&buf->rd, rd);
		return 0;
	}

	// Handle wrapping seeks
	const int size = buf->size;
	const int s_max = size - (rd - (wr-1));
	if (n > s_max) {
		return -1;
	}
	rd += n;
	rd %= buf->size;
	buf->n_read += n;
	atomic_store(&buf->rd, rd);
	return 0;
}
