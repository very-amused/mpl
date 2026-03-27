#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>

#include <Windows.h>
#include <consoleapi.h>
#include <processenv.h>
#include <handleapi.h>
#include <synchapi.h>
#include <consoleapi2.h>

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

	HANDLE input; // Console input handle
	HANDLE shutdown_evt; // Shutdown event handle used to stop the input loop

	 pthread_t thread;
	 _Atomic(enum InputMode) mode; // Input Mode
};

// Block until a character is read from *thr.
// poll() is useless on win32, so the only way to break this is with pthread_cancel
static char InputThread_getchar(InputThread *thr) {
	// NOTE: WaitForMultipleObjects will return the lowest index corresponding to a signal object.
	// We want to be responsive to a shutdown signal, so we place it first in the array
	HANDLE object_handles[] = {thr->shutdown_evt, thr->input};
	static const DWORD n_handles = sizeof(object_handles) / sizeof(object_handles[0]);

	char input_key = '\0';
	while (!input_key) {
		LOG(Verbosity_DEBUG, "Blocking input on WaitForMultipleObjects...\n");
		DWORD status = WaitForMultipleObjects(n_handles, object_handles, false, INFINITE);
		LOG(Verbosity_DEBUG, "Done with WaitForMultipleObjects...\n");

			
		// This is verbose but it's better to be API-compliant than rely on UB
		if (!(status >= WAIT_OBJECT_0 && status <= WAIT_OBJECT_0+(n_handles-1))) {
			LOG(Verbosity_NORMAL, "WaitForMultipleObjects failed. This should never happen\n");
			continue;
		}
		const DWORD signal_index = status - WAIT_OBJECT_0;
		if (signal_index == 0) {
			LOG(Verbosity_DEBUG, "Got shutdown signal in InputThread_getchar\n");
			return EOF;
		}

		DWORD n_read;
		LOG(Verbosity_DEBUG, "Blocking on ReadConsole...\n");
		ReadConsoleA(thr->input, &input_key, sizeof(input_key), &n_read, NULL);
		// We have to flush the input buffer to avoid the keypress event re-firing (yes, it is bad that we have to do this)
		FlushConsoleInputBuffer(thr->input);
		LOG(Verbosity_DEBUG, "Done with ReadConsole...\n");
	}


	return input_key;
}

static void *InputThread_loop(void *thr__) {
	InputThread *thr = thr__;

	// Uncook terminal input
	DWORD console_mode;
	bool ok = GetConsoleMode(thr->input, &console_mode);
	if (!ok) {
		LOG(Verbosity_NORMAL, "Failed to get console mode, bailing out\n");
		return NULL;
	}
	const DWORD orig_console_mode = console_mode;
	console_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT); // TODO: check if this does what ~(ECHO | ICANON) does on linux
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
			LOG(Verbosity_DEBUG, "Read keypress `%c`, sending to EventQueue\n", input_key);
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
	LOG(Verbosity_VERBOSE, "Input thread received cancel signal\n");
	ok = SetConsoleMode(thr->input, orig_console_mode);
	if (!ok) {
		LOG(Verbosity_NORMAL, "Failed to reset console mode\n");
	}
	return NULL;
}

InputThread *InputThread_new(EventQueue *eq) {
	InputThread *thr = malloc(sizeof(InputThread));
	thr->eq = eq;
	thr->mode = InputMode_KEY;
	thr->input = GetStdHandle(STD_INPUT_HANDLE);
	// ref https://learn.microsoft.com/en-us/windows/win32/sync/using-event-objects
	thr->shutdown_evt = CreateEventA(NULL, true, false, NULL);

	// Start input thread
	pthread_create(&thr->thread, NULL, InputThread_loop, thr);

	return thr;
}

void InputThread_free(InputThread *thr) {
	LOG(Verbosity_VERBOSE, "InputThread_free called\n");
	// Send stop signal to thread
	if (!SetEvent(thr->shutdown_evt)) {
		LOG(Verbosity_NORMAL, "Failed to signal InputThread shutdown. Shutdown might hang from here on.");
	}
	LOG(Verbosity_VERBOSE, "SetEvent called (shutdown signaled)\n");

	// Join thread so we can safely free its resources
	pthread_join(thr->thread, NULL);
	LOG(Verbosity_DEBUG, "pthread_join exited\n");

	// Free thread resources
	CloseHandle(thr->input);
	CloseHandle(thr->shutdown_evt);
	free(thr);
}
