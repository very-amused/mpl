#include "fmt.h"
#include "config/parse_v2/types.h"
#include "track_queue/queue.h"

#include <stdarg.h>
#include <stdio.h>


int fmt_printf(Formatter *fmt, const char *format, ...) {
	va_list args;
	va_start(args, format);
	int n = fmt->vwrite(fmt->ctx, format, args);
	va_end(args);

	return n;
}

int fmt_data(Formatter *fmt, const ConfigVal *val) {
	// Write type as header
	fmt_printf(fmt, "%s:\n", ConfigType_pretty_name(val->type));

	// Format data
	switch (val->type) {
	case Config_I32:
		return fmt_printf(fmt, "%d\n", val->val_i32);
	case Config_BOOL:
		return fmt_printf(fmt, "%s\n", val->val_bool ? "true" : "false");
	case Config_STR:
		return fmt_printf(fmt, "%s\n", val->val_str);

	case Config_TRACK_QUEUE:
		return TrackQueue_fmt(val->val_ptr, fmt);
	}

	return -1;
}

/* CLI formatting */

static int fmt_cli_vwrite(void *ctx, const char *format, va_list args) {
	return vfprintf(stderr, format, args);
}

Formatter FMT_CLI = {
	.vwrite = fmt_cli_vwrite
};
