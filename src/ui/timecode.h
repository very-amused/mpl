#pragma once
#include "audio/pcm.h"
#include "event.h"

#include <stddef.h>

/* Utilities for interpreting and displaying track timecodes and durations */

// Format a timecode for display
void fmt_timecode(char *buf, size_t buf_max, EventBody_Timecode timecode, const AudioPCM *pcm);
// Format a timecode for display, including milliseconds
void fmt_timecode_ms(char *buf, size_t buf_max, EventBody_Timecode timecode, const AudioPCM *pcm);
