#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>

#include <windows.h>
#include <consoleapi.h>
#include <processenv.h>
#include <handleapi.h>

// FIXME: we need to soft exit. InputThread_loop is hanging forever since it never hits a recognized async-safe cancellation point

// NOTE: We can't poll on win32, so we rely on pthread_cancel instead
#include "error.h"
#include "input_thread.h"
#include "event_queue.h"
#include "ui/event.h"
#include "util/log.h"

struct InputThread {
	// EventQueue for sending input events
	EventQueue *eq;

	HANDLE input;
	DWORD orig_console_mode; // Used to reset console in InputThread_free

	 pthread_t thread;
	 _Atomic(enum InputMode) mode; // Input Mode
};

// Block until a character is read from *thr.
// poll() is useless on win32, so the only way to break this is with pthread_cancel
static char InputThread_getchar(InputThread *thr) {
	char out;
	DWORD n_read;

	bool ok = ReadConsole(thr->input, &out, sizeof(out), &n_read, NULL);
	if (!ok || n_read != sizeof(out)) {
		LOG(Verbosity_NORMAL, "Error: failed to read next char (ReadConsole)\n");
		out = '\0';
	}

	return out;
}

static void *InputThread_loop(void *thr__) {
	InputThread *thr = thr__;

	// Uncook terminal input
	// TODO: recook in InputThread_free
	DWORD console_mode;
	bool ok = GetConsoleMode(thr->input, &console_mode);
	if (!ok) {
		LOG(Verbosity_NORMAL, "Failed to get console mode, bailing out\n");
		return NULL;
	} 
	thr->orig_console_mode = console_mode;
	console_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT); // TODO: check if this does what ~(ECHO | ICANON) does on linux
	ok = SetConsoleMode(thr->input, console_mode);
	if (!ok) {
		LOG(Verbosity_NORMAL, "Failed to set console mode. Things could get weird from here.\n");
	}

	while (true) {
		// Get user input
		const enum InputMode mode = atomic_load(&thr->mode);
		switch (mode) {
		case InputMode_KEY:
		{
			EventBody_Keypress input_key = InputThread_getchar(thr);
			if (input_key == EOF) {
				goto cancel;
			}
			Event evt = {.event_type = mpl_KEYPRESS, .body_size = sizeof(EventBody_Keypress), .body_inline = input_key};
			EventQueue_send(thr->eq, &evt);
			break;
		}
		case InputMode_TEXT:
			LOG(Verbosity_NORMAL, "InputMode_TEXT is not yet implemented!\n");
			continue;
		}
	}

cancel:
	LOG(Verbosity_VERBOSE, "Input thread received cancel signal.\n");
	ok = SetConsoleMode(thr->input, thr->orig_console_mode);
	if (!ok) {
		LOG(Verbosity_NORMAL, "Failed to reset console mode.\n");
	}
	return NULL;
}

InputThread *InputThread_new(EventQueue *eq) {
	InputThread *thr = malloc(sizeof(InputThread));
	thr->eq = eq;
	thr->mode = InputMode_KEY;
	thr->input = GetStdHandle(STD_INPUT_HANDLE);

	// Start input thread
	pthread_create(&thr->thread, NULL, InputThread_loop, thr);

	return thr;
}

void InputThread_free(InputThread *thr) {
	// no poll on windows, so we just have to slam the brakes
	pthread_cancel(thr->thread);
	// Uncook input
	LOG(Verbosity_DEBUG, "pthread_cancel exited\n");

	// Join thread so we can safely free its resources
	pthread_join(thr->thread, NULL);
	LOG(Verbosity_DEBUG, "pthread_join exited\n");

	// Free thread resources
	CloseHandle(thr->input);
	free(thr);
}
