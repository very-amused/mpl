#include "timestamp.h"
#include <stdint.h>
#include <stdio.h>

void fmt_timestamp(char *buf, size_t buf_max, float timestamp) {
	static const uint32_t DAY = 24 * 60 * 60;
	static const uint32_t HR = 60 * 60;
	static const uint32_t MIN = 60;

	uint32_t seconds = timestamp; 
	uint32_t days = seconds / DAY;
	seconds -= days * DAY;
	uint32_t hours = seconds / HR;
	seconds -= hours * HR;
	uint32_t minutes = seconds / MIN;
	seconds -= minutes * MIN;

	if (days > 0) {
		snprintf(buf, buf_max, "%02d:%02d:%02d:%02d", days, hours, minutes, seconds);
		return;
	}
	if (hours > 0) {
		snprintf(buf, buf_max, "%02d:%02d:%02d", hours, minutes, seconds);
		return;
	}
	snprintf(buf, buf_max, "%02d:%02d", minutes, seconds);
}

void fmt_timestamp_ms(char *buf, size_t buf_max, float timestamp) {
	static const uint32_t DAY = 24 * 60 * 60;
	static const uint32_t HR = 60 * 60;
	static const uint32_t MIN = 60;

	uint32_t seconds = timestamp; 
	uint32_t days = seconds / DAY;
	seconds -= days * DAY;
	uint32_t hours = seconds / HR;
	seconds -= hours * HR;
	uint32_t minutes = seconds / MIN;
	seconds -= minutes * MIN;

	float rem = timestamp - (uint32_t)(timestamp);
	int ms = rem * 1000;

	if (days > 0) {
		snprintf(buf, buf_max, "%02d:%02d:%02d:%02d.%03d", days, hours, minutes, seconds, ms);
		return;
	}
	if (hours > 0) {
		snprintf(buf, buf_max, "%02d:%02d:%02d.%03d", hours, minutes, seconds, ms);
		return;
	}
	snprintf(buf, buf_max, "%02d:%02d.%03d", minutes, seconds, ms);
}
