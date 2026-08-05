#include "ui/fmt.h"
#include "track_meta.h"
#include <string.h>
#include <stdlib.h>

void TrackMeta_init(TrackMeta *meta) {
	memset(meta, 0, sizeof(TrackMeta));
}

void TrackMeta_deinit(TrackMeta *meta) {
	free(meta->name);
	free(meta->artist);
	free(meta->album);
}

int TrackMeta_fmt(const TrackMeta *meta, Formatter *fmt) {
	int n = 0; // bytes written

	// Display track metadata
	static const char TERM_BOLD[] = "\x1b[1m";
	static const char TERM_ITAL[] = "\x1b[3m";
	static const char TERM_RESET[] = "\x1b[0m";
	if (meta->artist) {
		n += fmt_printf(fmt, "%sArtist:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, meta->artist, TERM_RESET);
	}
	if (meta->name) {
		n += fmt_printf(fmt, "%sTitle:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, meta->name, TERM_RESET);
	}
	if (meta->album) {
		n += fmt_printf(fmt, "%sAlbum:%s %s%s%s\n", TERM_BOLD, TERM_RESET,
				TERM_ITAL, meta->album, TERM_RESET);
	}

	return n;
}
