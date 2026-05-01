#include "settings.h"

#include <string.h>
#include <stdlib.h>

void Settings_init(Settings *opts) {
	memcpy(opts, &default_settings, sizeof(Settings));
}

void Settings_deinit(Settings *opts) {
	free(opts->audio_backend);
	free(opts->user_interface);
}
