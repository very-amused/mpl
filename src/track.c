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
	t->name_len = 0;
	t->name = NULL;

	// TODO: get artist from url
	t->artist_len = 0;
	t->artist = NULL;

	// TODO: get album from url
	t->album_len = 0;
	t->album = NULL;

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

float Track_timestamp(const Track *t) {
	if (!t->audio) {
		return 0;
	}

	return AudioPCM_seconds(&t->audio->pcm, atomic_load(&t->audio->buffer->n_read));
}
