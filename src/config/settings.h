#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Settings configurable in mpl.conf
typedef struct Settings {
	uint32_t at_buffer_ahead; // number of seconds to buffer ahead for each track

	char *audio_backend; // Name of audio backend to use (e.g "pulseaudio", "pipewire", "wasapi", "fast")
	uint32_t ab_buffer_ms; // number of ms to buffer with the audio backend (i.e pulseaudio)

	char *user_interface; // Name of user interface to use (e.g "cli")
	bool ui_timecode_ms; // Display milliseconds in timecodes (i.e track position)
} Settings;

// Default values for all settings
static const Settings default_settings = {
	.at_buffer_ahead = 30,

	.audio_backend = NULL, // use default AudioBackened
	.ab_buffer_ms = 100,

	.user_interface = NULL, // use default UserInterface
	.ui_timecode_ms = false
};

// Initialize settings to default values
void Settings_init(Settings *opts);
// Deinitialize settings for freeing
void Settings_deinit(Settings *opts);
