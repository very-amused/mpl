#pragma once
#include <stddef.h>

#define av_perror(e, buf) av_strerror(e, buf, sizeof(buf)); \
	fprintf(stderr, "libav error: %s\n", buf)

#define ENUM_KEY(NAME) case NAME: return #NAME;
#define ENUM_VAL(NAME) NAME,

static const char DEFAULT_ERR_NAME[] = "Not implemented.";

#define AUDIOBACKEND_ERR(VARIANT) \
	VARIANT(AudioBackend_OK) \
	VARIANT(AudioBackend_EVENT_QUEUE_ERR) \
	VARIANT(AudioBackend_BAD_ALLOC) \
	VARIANT(AudioBackend_CONNECT_ERR) \
	VARIANT(AudioBackend_LOOP_STALL) \
	VARIANT(AudioBackend_STREAM_ERR) \
	VARIANT(AudioBackend_STREAM_EXISTS) \
	VARIANT(AudioBackend_FB_WRITE_ERR) \
	VARIANT(AudioBackend_PLAY_ERR)

/* An error returned by AudioBackend_* methods.
 *
 * NOTE: AudioBackend_STREAM_EXISTS denotes that AudioBackend_prepare(), which initializes a new audio stream,
 * was called with an existing (non-terminated) stream present.
 * This error indicates that the caller must instead use AudioBackend_queue() in this situation to hot-queue.
 */
enum AudioBackend_ERR {
	AUDIOBACKEND_ERR(ENUM_VAL)
};

static inline const char *AudioBackend_ERR_name(enum AudioBackend_ERR err) {
	switch (err) {
		AUDIOBACKEND_ERR(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef AUDIOBACKEND_ERR


// ref: https://kubyshkin.name/posts/c-language-enums-tips-and-tricks/
#define AUDIOTRACK_ERR(VARIANT) \
	VARIANT(AudioTrack_OK) \
	VARIANT(AudioTrack_EOF) \
	VARIANT(AudioTrack_NOT_FOUND) \
	VARIANT(AudioTrack_IO_ERR) \
	VARIANT(AudioTrack_NO_STREAM) \
	VARIANT(AudioTrack_NO_DECODER) \
	VARIANT(AudioTrack_BUFFER_ERR) \
	VARIANT(AudioTrack_BAD_ALLOC) \
	VARIANT(AudioTrack_CODEC_ERR) \
	VARIANT(AudioTrack_PACKET_ERR) \
	VARIANT(AudioTrack_FRAME_ERR)

// Errors returned by an AudioTrack_* method
enum AudioTrack_ERR {
	AUDIOTRACK_ERR(ENUM_VAL)
};

static inline const char *AudioTrack_ERR_name(enum AudioTrack_ERR err) {
	switch (err) {
		AUDIOTRACK_ERR(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef AUDIOTRACK_ERR

#define AUDIOBUFFER_ERR(VARIANT) \
	VARIANT(AudioBuffer_OK) \
	VARIANT(AudioBuffer_SEEK_OOB) \
	VARIANT(AudioBuffer_INVALID_SEEK)

// Errors returned by certain AudioBuffer_* methods
enum AudioBuffer_ERR {
	AUDIOBUFFER_ERR(ENUM_VAL)
};

static inline const char *AudioBuffer_ERR_name(enum AudioBuffer_ERR err) {
	switch (err) {
		AUDIOBUFFER_ERR(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef AUDIOBUFFER_ERR

#define VERBOSITY(VARIANT) \
	VARIANT(Verbosity_NORMAL) \
	VARIANT(Verbosity_VERBOSE) \
	VARIANT(Verbosity_DEBUG)

/* Verbosity levels for logging facilities.
 * Currently only used in UserInterface_CLI */
enum Verbosity {
	VERBOSITY(ENUM_VAL)
};

static inline const char *Verbosity_name(enum Verbosity lvl) {
	switch (lvl) {
		VERBOSITY(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef VERBOSITY

#define KEYBIND_ERR(VARIANT) \
	VARIANT(Keybind_OK) \
	VARIANT(Keybind_NOT_FOUND) /* A binding was not found for the provided key */ \
	VARIANT(Keybind_SYNTAX_ERR) /* Syntax error while parsing a keybind definition */ \
	VARIANT(Keybind_INVALID_FN) /* A function was called that doesn't exist */ \
	VARIANT(Keybind_INVALID_ARG) /* An invalid argument was provided in the binding definition */ \
	VARIANT(Keybind_BINDING_CONFLICT) /* A key that was already bound cannot be bound again without explicitly rebinding it */ \
	VARIANT(Keybind_NON_ASCII) /* Non-ASCII codepoint provided when MPL was built with the 'ascii_keybinds' feature */

// Errors returned by a KeybindMap_* method
enum Keybind_ERR {
	KEYBIND_ERR(ENUM_VAL)
};

static inline const char *Keybind_ERR_name(enum Keybind_ERR err) {
	switch (err) {
		KEYBIND_ERR(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef KEYBIND_ERR

#define SETTINGS_ERR(VARIANT) \
	VARIANT(Settings_OK) \
	VARIANT(Settings_UNKNOWN) /* An unknown setting is provided in mpl.conf */ \
	VARIANT(Settings_SYNTAX_ERR) /* Syntax error while parsing a setting definition */ \

// Errors return by a Settings_* method
enum Settings_ERR {
	SETTINGS_ERR(ENUM_VAL)
};

static inline const char *Settings_ERR_name(enum Settings_ERR err) {
	switch (err) {
		SETTINGS_ERR(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef SETTINGS_ERR
