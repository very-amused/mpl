/* MPL CLI user interface */
#include "cli.h"

#include <semaphore.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

// InputMode tells the UI how to collect input data
enum InputMode {
	InputMode_KEY, // Get one key of input at a time without buffering
	InputMode_TEXT // Get buffered text input
};

struct MPL_CLI {
	// Thread that runs a blocking input loop and communicates InputEvent's through the event_queue.
	pthread_t *input_thread;
	sem_t cancel; // Tell input_thread to exit

	// Settable from the input thread only.
	enum InputMode input_mode;	
};

void *MPL_CLI_input_loop(void *input_events__);

MPL_CLI *MPL_CLI_new() {
	MPL_CLI *cli = malloc(sizeof(MPL_CLI));
	cli->input_mode = InputMode_KEY;
	cli->input_thread = NULL;
	return cli;
}

void MPL_CLI_free(MPL_CLI *cli) {
	// Shutdown input thread
	if (cli->input_thread) {
		sem_post(&cli->cancel);
		pthread_join(*cli->input_thread, NULL);
	}
	free(cli->input_thread);
}
