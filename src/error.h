#pragma once
#include <stddef.h>

#define av_perror(e, buf) av_strerror(e, buf, sizeof(buf)); \
	fprintf(stderr, "libav error: %s\n", buf)

#define ENUM_KEY(NAME) case NAME: return #NAME;
#define ENUM_VAL(NAME) NAME,

#define AUDIOBACKEND_ERR(VARIANT) \
	VARIANT(AudioBackend_OK) \
	VARIANT(AudioBackend_BAD_ALLOC) \
	VARIANT(AudioBackend_CONNECT_ERR) \
	VARIANT(AudioBackend_LOOP_ERR) \
	VARIANT(AudioBackend_STREAM_ERR) \
	VARIANT(AudioBackend_STREAM_EXISTS) \
	VARIANT(AudioBackend_FB_WRITE_ERR)

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
	return NULL;
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
	return NULL;
}
#undef AUDIOTRACK_ERR
