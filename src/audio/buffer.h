#pragma once
#include <stddef.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>
#include <stdio.h>
#include <stdatomic.h>

// Sampling parameters used to interpret decoded PCM frames
typedef struct AudioPCM {
	enum AVSampleFormat sample_fmt;
	uint32_t sample_rate;
	uint8_t n_channels;
} AudioPCM;

// A ring buffer used to hold decoded PCM samples
typedef struct AudioBuffer {
	// The raw byte size of the buffer.
	ssize_t buf_size;
	// The 'line size' of the buffer. if is_planar is true,
	// this is the size of one channel's worth of consecutive samples.
	// Otherwise, this is equal to buf_size and all channels' worth of interleaved samples.
	// NOTE: The buffer wraps mod line_size, NOT mod buf_size.
	int line_size;

	unsigned char *data;
	atomic_int rd, wr; // Read/write indices relative to line_size
} AudioBuffer;

// Initialize an AudioBuffer for use
int AudioBuffer_init(AudioBuffer *ab, const AudioPCM *pcm);
// Deinitialize an AudioBuffer for freeing
void AudioBuffer_deinit(AudioBuffer *ab);

// Write n bytes from *src to *ab. Never blocks.
// Returns the number of bytes actually written.
size_t AudioBuffer_write(AudioBuffer *ab, unsigned char *src, size_t n);
// Read n bytes from *ab to *dst. Never blocks.
// Returns the number of bytes actually read.
size_t AudioBuffer_read(AudioBuffer *ab, unsigned char *dst, size_t n);
