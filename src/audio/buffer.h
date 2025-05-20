#pragma once
#include "pcm.h"
#include "seek.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <libavutil/samplefmt.h>
#include <stdio.h>
#include <stdatomic.h>
#include <semaphore.h>


// A ring buffer used to hold decoded PCM samples
struct RingBuffer {
	size_t size; // Total size of the buffer in bytes
	size_t frame_size; // Frame size, for convenient computation of the number of frames read

	unsigned char *data;
	atomic_int rd, wr; // Read/write indices relative to line_size
	size_t n_read; // Cumulative number of bytes read since initialization
	size_t n_written; // Cumulative number of bytes written since initialization

	// Semaphores providing read/write notifications to minimize spinning
	sem_t rd_sem, wr_sem;
};
typedef struct RingBuffer AudioBuffer;

// Initialize an AudioBuffer for use
int AudioBuffer_init(AudioBuffer *buf, const AudioPCM *pcm);
// Deinitialize an AudioBuffer for freeing
void AudioBuffer_deinit(AudioBuffer *buf);

// Write up to n bytes from *src to *ab. Never blocks.
// Returns the number of bytes actually written.
size_t AudioBuffer_write(AudioBuffer *buf, unsigned char *src, size_t n);
// Read up to n bytes from *ab to *dst. Never blocks.
// Returns the number of bytes actually read.
//
// Iff align == 1 and the n parameter is a multiple of buf->frame_size,
// the returned number of bytes is guaranteed to also be a multiple of buf->frame_size.
size_t AudioBuffer_read(AudioBuffer *buf, unsigned char *dst, size_t n, bool align);
// Return the maximum size (in bytes) of a non-blocking read, optionally aligned to buf->frame_size
// NOTE: Pass -1 as rd and wr to automatically load rd/wr indices
const size_t AudioBuffer_max_read(const AudioBuffer *buf, int rd, int wr, bool align);

// Try to seek within an AudioBuffer.
// NOTE: the buffer must be de-facto locked via some external mechanism when this is called.
// The ideal way to achieve this is to lock the AudioBackend thread and have the BufferThread
// handle seeks.
//
// Returns 0 on success, or -1 when the seek cannot be done in-buffer.
enum AudioBuffer_ERR AudioBuffer_seek(AudioBuffer *buf, int64_t offset_bytes, enum AudioSeek seek_dir);
