#include <stdio.h>
#include "audio/backend.h"
#include "audio/backends.h"

int main(int argc, char **argv) {
	printf("This is MPL v0.0.0\n");

	if (argc < 2) {
		fprintf(stderr, "usage: mpl {file}\n");
		return 1;
	}
	char *file = argv[1];
	
	// Initialize audio backend
	AudioBackend ab = AB_PulseAudio;
	AudioBackend_init(&ab);

	// Cleanup
	AudioBackend_deinit(&ab);

	return 0;
}
