#pragma once

// Errors returned by an AudioTrack_* method
enum AudioTrack_ERR {
	AudioTrack_OK,
	AudioTrack_IO_ERR,
	AudioTrack_NO_STREAM,
	AudioTrack_NO_DECODER,
	AudioTrack_BUFFER_ERR,
	AudioTrack_BAD_ALLOC,
	AudioTrack_CODEC_ERR,
	AudioTrack_PACKET_ERR,
	AudioTrack_FRAME_ERR
};

#define av_perror(e, buf) av_strerror(e, buf, sizeof(buf)); \
	fprintf(stderr, "libav error: %s\n", buf)
