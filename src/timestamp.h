#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Utilities for interpreting and displaying track timestamps and durations */

// Format a timestamp for display (MM:SS, HH:MM:SS, etc)
void fmt_timestamp(char *buf, size_t buf_max, float timestamp);
// Format a timestamp for display (millisecond precision)
void fmt_timestamp_ms(char *buf, size_t buf_max, float timestamp);
