#include "queue/queue.h"
#include "queue/state.h"
#include "track.h"
#include "ui/cli.h"
#include "ui/event.h"
#include "ui/event_queue.h"
#include "ui/timecode.h"

#include <ctype.h>
#include <stdint.h>
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

	// Fire up CLI user interface and the main EventQueue
	UserInterface_CLI ui;
	UserInterface_CLI_init(&ui);
	fprintf(stderr, "Ready to receive input\n");

	// Initialize queue w/ track
	Queue queue;
	Queue_init(&queue);
	Queue_connect_audio(&queue, NULL, ui.evt_queue);
	Queue_prepend(&queue, Track_new(url, url_len)); // Takes ownership of *track
	Queue_play(&queue, 0);


	// Handle events on the main thread
	Event evt;
	while (EventQueue_recv(ui.evt_queue, &evt) == 0) {
		switch (evt.event_type) {
		case mpl_KEYPRESS:
		{
			EventBody_Keypress key = evt.body_inline;
			printf("\nKey %c was pressed\n", key);
			switch (tolower(key)) {
			case 'p':
				Queue_play(&queue, queue.playback_state == Queue_PLAYING);
				break;
			case 'q':
				goto quit;
			}
			break;
		}
		case mpl_TIMECODE:
		{
			EventBody_Timecode timecode = evt.body_inline;
			const Track *tr = Queue_cur_track(&queue);
			AudioPCM pcm = tr->audio->pcm;
			char timecode_buf[255];
			char duration_buf[255];
			fmt_timecode(timecode_buf, sizeof(timecode_buf), timecode, &pcm);
			fmt_timecode(duration_buf, sizeof(duration_buf), tr->audio->duration_timecode, &pcm); // FIXME
			static const char CLEAR_LINE_VT100[] = "\033[2K\r";
			fprintf(stderr, CLEAR_LINE_VT100);
			fprintf(stderr, "Timecode: %s/%s", timecode_buf, duration_buf);
			break;
		}
		default:
			fprintf(stderr, "Warning: unhandled event %s\n", MPL_EVENT_name(evt.event_type));
		}
	}
quit:

	// Cleanup
	Queue_deinit(&queue);
	UserInterface_CLI_deinit(&ui);
	free(url);

	return 0;
}
