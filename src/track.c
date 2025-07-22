#include "track.h"
#include "audio/track.h"
#include "track_meta.h"
#include "util/compat/string_win32.h"

#include <stdatomic.h>
#include <string.h>

Track *Track_new(const char *url, const size_t url_len) {
	Track *t = malloc(sizeof(Track));
	t->url_len = url_len;
	t->url = strndup(url, url_len);

	TrackMeta_init(&t->meta);

	t->audio = NULL;

	return t;
}
void Track_free(Track *t) {
	free(t->url);
	t->url = NULL;
	t->url_len = 0;
	if (t->audio) {
		AudioTrack_deinit(t->audio);
		free(t->audio);
	}
	TrackMeta_deinit(&t->meta);
	free(t);
}
