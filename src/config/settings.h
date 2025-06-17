#pragma once
#include "error.h"

#include <stdbool.h>
#include <stdint.h>

// Settings configurable in mpl.conf
typedef struct Settings {
	uint32_t at_buffer_ahead; // number of seconds to buffer ahead for each track

	uint32_t ab_buffer_ms; // number of ms to buffer with the audio backend (i.e pulseaudio)
} Settings;

// Default values for all settings
static const Settings default_settings = {
	.at_buffer_ahead = 30,

	.ab_buffer_ms = 100
};

// Initialize settings to default values
void Settings_init(Settings *opts);
// Deinitialize settings for freeing
void Settings_deinit(Settings *opts);

// Parse a setting definition of the form
// {opt} = {val}
enum Settings_ERR Settings_parse_setting(Settings *opts, const char *line);

#define SETTINGS_TYPE(VARIANT) \
	VARIANT(Settings_U32)

// The type of a settings value
enum Settings_t {
	SETTINGS_TYPE(ENUM_VAL)
};

static const inline char *Settings_t_name(enum Settings_t t) {
	switch (t) {
		SETTINGS_TYPE(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}


#undef SETTINGS_TYPE
