#pragma once
#include <stddef.h>

#define av_perror(e, buf) av_strerror(e, buf, sizeof(buf)); \
	fprintf(stderr, "libav error: %s\n", buf)

#define ENUM_KEY(NAME) case NAME: return #NAME;
#define ENUM_VAL(NAME) NAME,

// ref: https://kubyshkin.name/posts/c-language-enums-tips-and-tricks/
#define AUDIOTRACK_ERR_ENUM(VARIANT) \
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
	AUDIOTRACK_ERR_ENUM(ENUM_VAL)
};

static inline const char *AudioTrack_ERR_name(enum AudioTrack_ERR err) {
	switch (err) {
		AUDIOTRACK_ERR_ENUM(ENUM_KEY)
	}
	return NULL;
}
#undef AUDIOTRACK_ERR_ENUM

// TODO: define more backend errors
#define AUDIOBACKEND_ERR_ENUM(VARIANT) \
	VARIANT(AudioBackend_OK) \
	VARIANT(AudioBackend_FB_WRITE_ERR)

enum AudioBackend_ERR {
	AUDIOBACKEND_ERR_ENUM(ENUM_VAL)
};

static inline const char *AudioBackend_ERR_name(enum AudioBackend_ERR err) {
	switch (err) {
		AUDIOBACKEND_ERR_ENUM(ENUM_KEY)
	}
	return NULL;
}
#undef AUDIOBACKEND_ERR_ENUM

#undef ENUM_VAL
#undef ENUM_KEY
