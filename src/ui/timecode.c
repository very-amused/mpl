#include "timecode.h"
#include <stdint.h>
#include <stdio.h>

static const uint32_t DAY = 24 * 60 * 60;
static const uint32_t HR = 60 * 60;
static const uint32_t MIN = 60;

void fmt_timecode(char *buf, size_t buf_max, EventBody_Timecode timecode, const AudioPCM *pcm) {
	// Separate timecode into fields
	uint32_t seconds = timecode / pcm->sample_rate; 
	uint32_t days = seconds / DAY;
	seconds -= days * DAY;
	uint32_t hours = seconds / HR;
	seconds -= hours * HR;
	uint32_t minutes = seconds / MIN;
	seconds -= minutes * MIN;

	if (days > 0) {
		snprintf(buf, buf_max, "%02d:%02d:%02d:%02d", days, hours, minutes, seconds);
	} else if (hours > 0) {
		snprintf(buf, buf_max, "%02d:%02d:%02d", hours, minutes, seconds);
	} else {
		snprintf(buf, buf_max, "%02d:%02d", minutes, seconds);
	}
}

void fmt_timecode_ms(char *buf, size_t buf_max, EventBody_Timecode timecode, const AudioPCM *pcm) {
	// Separate timecode into fields
	uint32_t seconds = timecode / pcm->sample_rate; 
	uint32_t days = seconds / DAY;
	seconds -= days * DAY;
	uint32_t hours = seconds / HR;
	seconds -= hours * HR;
	uint32_t minutes = seconds / MIN;
	seconds -= minutes * MIN;

	double seconds_d = (double)timecode / pcm->sample_rate;
	double rem = seconds_d  - (uint32_t)seconds_d;
	uint32_t ms = rem * 1000;

	if (days > 0) {
		snprintf(buf, buf_max, "%02d:%02d:%02d:%02d.%03d", days, hours, minutes, seconds, ms);
	} else if (hours > 0) {
		snprintf(buf, buf_max, "%02d:%02d:%02d.%03d", hours, minutes, seconds, ms);
	} else {
		snprintf(buf, buf_max, "%02d:%02d.%03d", minutes, seconds, ms);
	}
}
