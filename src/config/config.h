#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Settings configure MPL */
typedef struct ConfigSettings {
	uint32_t buffer_len;
	char *audio_backend; // options: "auto", "pulseaudio"
} ConfigSettings;

static const ConfigSettings DefaultSettings = {
	.buffer_len = 30000, // 30 seconds
	.audio_backend = "auto"
};

/* Actions control MPL directly */

typedef struct ConfigActions {
	void (*play)(bool play);
	void (*quit)();
	void (*seek)(uint32_t ms);
	void (*seek_snap)(uint32_t ms);
} ConfigActions;

/* Globals expose MPL's state to mpl.conf, enabling dynamic actions (i.e toggles) */

typedef struct ConfigGlobals {
	bool is_playing;
} ConfigGlobals;

/* Bindings bind keys to actions */
// ...we need a hash map

typedef struct Config {
	ConfigSettings settings;
	ConfigActions actions;
	ConfigGlobals globals;
} Config;
