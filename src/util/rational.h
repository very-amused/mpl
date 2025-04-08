#pragma once

// mplRational - A rational datatype with 64 bit fields
#include <libavutil/rational.h>
#include <stdint.h>
typedef struct mplRational {
	int64_t num, den;
} mplRational;

// Convert an AVRational to an mplRational
void mplRational_from_AVRational(mplRational *rat, AVRational avrat);

// Reduce an mplRational to its simplest form
void mplRational_reduce(mplRational *rat);

// Convert an mplRational to a double precision floating point value
double mplRational_d(const mplRational *rat);
