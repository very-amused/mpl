#include "queue/queue.h"
#include "track.h"

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

	// Initialize queue w/ track
	Queue queue;
	Queue_init(&queue);
	Queue_connect_audio(&queue, NULL);
	Queue_prepend(&queue, Track_new(url, url_len)); // Takes ownership of *track

	Queue_play(&queue, 0);

	// Cleanup
	Queue_deinit(&queue);
	free(url);

	return 0;
}
