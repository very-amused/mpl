#include "track.h"
#include "audio/pcm.h"
#include "audio/track.h"
#include <string.h>
#include <stdatomic.h>

Track *Track_new(const char *url, const size_t url_len) {
	Track *t = malloc(sizeof(Track));
	t->url_len = url_len;
	t->url = strndup(url, url_len);

	// TODO: Get track name from url
	memset(&t->meta, 0, sizeof(t->meta));

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
	free(t);
}
