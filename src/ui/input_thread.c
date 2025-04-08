#include "input_thread.h"
#include "event_queue.h"
#include "ui/event.h"

#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>


struct InputThread {
	// EventQueue for sending input events
	EventQueue *eq;

	int input_fileno;
	FILE *input;
	pthread_t thread;
	sem_t cancel; // Tell the thread to exit
	_Atomic(enum InputMode) mode; // Input Mode
};

void *InputThread_loop(void *thr__) {
	InputThread *thr = thr__;
	// Tell the terminal we want to receive input without line buffering
	struct termios term_opts;
	tcgetattr(STDIN_FILENO, &term_opts);
	const struct termios orig_term_opts = term_opts;
	term_opts.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSANOW, &term_opts);

	while (true) {
		// Handle cancel signal
		int cancel;
		sem_getvalue(&thr->cancel, &cancel);
		if (cancel) {
			tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_opts);
			pthread_exit(NULL);
		}

		// Get user input
		const enum InputMode mode = atomic_load(&thr->mode);
		switch (mode) {
		case InputMode_KEY:
		{
			EventBody_Keypress input_key = getchar();
			Event evt = {.event_type = mpl_KEYPRESS, .body_size = sizeof(EventBody_Keypress), .body_inline = input_key};
			EventQueue_send(thr->eq, &evt);
			if (tolower(input_key) == 'q') {
				sem_post(&thr->cancel);
			}
			break;
		}
		case InputMode_TEXT:
			fprintf(stderr, "InputMode_TEXT is not yet implemented!\n");
			continue;
		}
	}
}

InputThread *InputThread_new(EventQueue *eq) {
	InputThread *thr = malloc(sizeof(InputThread));
	thr->eq = eq;
	thr->mode = InputMode_KEY;

	// Initialize cancel semaphore
	sem_init(&thr->cancel, 0, 0);
	// Start input thread
	pthread_create(&thr->thread, NULL, InputThread_loop, thr);

	return thr;
}
void InputThread_free(InputThread *thr) {
	// Stop thread
	sem_post(&thr->cancel);
	pthread_join(thr->thread, NULL);
	free(thr);
}

void InputThread_set_mode(InputThread *thr, enum InputMode mode) {
	atomic_store(&thr->mode, mode);
}
