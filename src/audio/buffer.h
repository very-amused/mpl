#pragma once
#include "pcm.h"

#include <stddef.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>
#include <stdio.h>
#include <stdatomic.h>
#include <semaphore.h>


// A ring buffer used to hold decoded PCM samples
typedef struct RingBuffer {
	size_t size; // Total size of the buffer in bytes
	size_t frame_size; // Frame size, for convenient computation of the number of frames read

	unsigned char *data;
	atomic_int rd, wr; // Read/write indices relative to line_size
	size_t n_read; // Cumulative number of bytes read since initialization

	// Semaphores providing read/write notifications to minimize spinning
	sem_t rd_sem, wr_sem;
} RingBuffer;

// Initialize an AudioBuffer for use
int AudioBuffer_init(RingBuffer *buf, const AudioPCM *pcm);
// Deinitialize an AudioBuffer for freeing
void AudioBuffer_deinit(RingBuffer *buf);

// Write n bytes from *src to *ab. Never blocks.
// Returns the number of bytes actually written.
size_t AudioBuffer_write(RingBuffer *buf, unsigned char *src, size_t n);
// Read n bytes from *ab to *dst. Never blocks.
// Returns the number of bytes actually read.
size_t AudioBuffer_read(RingBuffer *buf, unsigned char *dst, size_t n);
