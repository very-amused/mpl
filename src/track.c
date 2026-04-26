#include "track.h"
#include "audio/track.h"
#include "error.h"
#include "track_meta.h"
#include "util/compat/string_win32.h"
#include "util/log.h"

#include <stdatomic.h>
#include <string.h>

Track *Track_new(const char *url, const size_t url_len, AudioBackend *ab) {
	Track *t = malloc(sizeof(Track));
	CHECK_ALLOC(t, NULL);
	t->url_len = url_len;
	t->url = strndup(url, url_len);

	// Initialize track audio (which also decodes streams needed for metadata)
	enum AudioTrack_ERR at_err = AudioTrack_init(&t->audio, t->url, ab);
	if (at_err != AudioTrack_OK) {
		LOG(Verbosity_NORMAL, "Failed to initialize AudioTrack %s: %s\n", t->url, AudioTrack_ERR_name(at_err));
	}

	TrackMeta_init(&t->meta);
	at_err = AudioTrack_get_metadata(&t->audio, &t->meta);
	if (at_err != AudioTrack_OK) {
		LOG(Verbosity_DEBUG, "Failed to get metadata for AudioTrack %s: %s\n", t->url, AudioTrack_ERR_name(at_err));
	}

	return t;
}
void Track_free(Track *t) {
	free(t->url);
	t->url = NULL;
	t->url_len = 0;
	AudioTrack_deinit(&t->audio);
	TrackMeta_deinit(&t->meta);
	free(t);
}
