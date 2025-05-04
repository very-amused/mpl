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
