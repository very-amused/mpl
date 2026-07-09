#pragma once

#include <stdarg.h>
#include <stddef.h>

#include "config/parse_v2/types.h"

/* Struct formatting for debug/eval output purposes */

typedef struct Formatter {
	int (*vwrite)(void *ctx, const char *format, va_list args); // Write formatted output

	void *ctx; // non-owned
} Formatter;

// Write a string to the fmt buffer
int fmt_printf(Formatter *fmt, const char *format, ...);

// Format data of a valid ConfigType and write it to the fmt buffer
int fmt_data(Formatter *fmt, const ConfigVal *val);

// A formatter that writes to stderr
extern Formatter FMT_CLI;
