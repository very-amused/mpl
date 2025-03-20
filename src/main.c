#include <stdio.h>
#include "audio/out/backend.h"
#include "audio/out/backends.h"
#include "track.h"
#include <unistd.h>

int main(int argc, char **argv) {
	printf("This is MPL v0.0.0\n");

	if (argc < 2) {
		fprintf(stderr, "usage: mpl {file}\n");
		return 1;
	}
	char *file = argv[1];

	// Initialize track
	Track track;
	
	// Initialize audio backend
	AudioBackend ab = AB_PulseAudio;
	if (AudioBackend_init(&ab) != 0) {
		return 1;
	}

	sleep(10);


	// Cleanup
	AudioBackend_deinit(&ab);

	return 0;
}
