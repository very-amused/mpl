#pragma once
#include <stddef.h>

// ref: https://kubyshkin.name/posts/c-language-enums-tips-and-tricks/
#define AUDIOTRACK_ERR_ENUM(VARIANT) \
	VARIANT(AudioTrack_OK) \
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
#define ENUM_VAL(NAME) NAME,
	AUDIOTRACK_ERR_ENUM(ENUM_VAL)
#undef ENUM_VAL
};

static inline const char *AudioTrack_ERR_name(enum AudioTrack_ERR err) {
#define ENUM_KEY(NAME) case NAME: return #NAME;
	switch (err) {
		AUDIOTRACK_ERR_ENUM(ENUM_KEY)
	}
#undef AUDIOTRACK_ERR_ENUM
	return NULL;
}

#define av_perror(e, buf) av_strerror(e, buf, sizeof(buf)); \
	fprintf(stderr, "libav error: %s\n", buf)
