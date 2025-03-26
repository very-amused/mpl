#include "audio/out/backend.h"
#include "audio/out/backends.h"
#include "audio/seek.h"
#include "audio/track.h"
#include "track.h"
#include "error.h"

#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv) {
	printf("This is MPL v0.0.0\n");

	if (argc < 2) {
		fprintf(stderr, "usage: mpl {file}\n");
		return 1;
	}
	// Form URL from file argv
	char *file = argv[1];
	static const char LIBAV_PROTO_FILE[] = "file:";
	static const size_t LIBAV_PROTO_FILE_LEN = sizeof(LIBAV_PROTO_FILE);
	const size_t url_len = LIBAV_PROTO_FILE_LEN + strlen(file);
	char *url = malloc((url_len + 1) * sizeof(char));
	snprintf(url, url_len, "%s%s", LIBAV_PROTO_FILE, file);

	// Initialize track
	Track track;
	Track_init(&track, url, url_len);
	track.audio = malloc(sizeof(AudioTrack));
	if (!track.audio) {
		fprintf(stderr, "Failed to allocate AudioTrack\n");
		return 1;
	}
	// Initialize track audio
	enum AudioTrack_ERR at_err = AudioTrack_init(track.audio, url);
	if (at_err != AudioTrack_OK) {
		fprintf(stderr, "Failed to initialize audio buffering for track %s, %s\n",
				url, AudioTrack_ERR_name(at_err));
		return 1;
	}

	// Buffer 5 seconds of audio data
	at_err = AudioTrack_buffer_ms(track.audio, AudioSeek_Relative, 5000);
	if (at_err != AudioTrack_OK) {
		fprintf(stderr, "Failed to fill initial buffer for track %s, %s\n",
				url, AudioTrack_ERR_name(at_err));
		return 1;
	}
	
	// Initialize audio backend
	AudioBackend ab = AB_PulseAudio;
	if (AudioBackend_init(&ab, &track.audio->pcm) != 0) {
		return 1;
	}

	// Play the track
	AudioBackend_prepare(&ab, track.audio);
	AudioBackend_play(&ab, 0);

	// Keep buffering data
	while (at_err == AudioTrack_OK) {
		at_err = AudioTrack_buffer_ms(track.audio, AudioSeek_Relative, 5000);
	}


	// Cleanup
	AudioBackend_deinit(&ab);
	AudioTrack_deinit(track.audio);
	Track_deinit(&track);
	free(url);

	return 0;
}
