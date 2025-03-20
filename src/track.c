#include "track.h"
#include <string.h>

void Track_init(Track *t, const char *url, const size_t url_len) {
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
}
void Track_deinit(Track *t) {
	free(t->url);
	t->url = NULL;
	t->url_len = 0;
}
