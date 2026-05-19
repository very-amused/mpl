#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include <Windows.h>
#include <consoleapi.h>
#include <processenv.h>
#include <synchapi.h>
#include <consoleapi2.h>
#include <fileapi.h>

#include "error.h"
#include "termio_thread.h"
#include "termio_events.h"
#include "track_queue/state.h"
#include "ui/event.h"
#include "util/log.h"

struct TermIOThread {
	// EventSubQueue for sending input events to the main thread
	EventSubQueue *evt_sq;
	// KeybindMap to implement shbind keybinds
	KeybindMap *keybinds;

	HANDLE input; // Console input handle
	HANDLE output; // Console output handle

	// Event pipe handles (used to receive TermIO_Event's)
	HANDLE evt_pipe_rd, evt_pipe_wr;
	HANDLE evt_pipe_sem; // Semaphore that counts events sent on evt_pipe

	// We need to keep track of these because
	// 1. when the timecode changes, we must already know the playback state
	// 2. when the playback state changes, we must already know the timecode
	char timecode_buf[255];
	char duration_buf[255];
	enum Queue_PLAYBACK_STATE playback_state;

	pthread_t thread;

	enum InputMode mode; // How we're processing input
	bool has_prompt;
	pthread_mutex_t mode_lock; // Lock over {mode} and {has_prompt}
};

static const char CONTINUE_IO_LOOP = -2;

// Update timecode and playback state in terminal output
static void write_playback_info(TermIOThread *thr, enum InputMode mode);

// Block until an input character is read
static char TermIOThread_getchar(TermIOThread *thr) {
	// NOTE: WaitForMultipleObjects will return the lowest index corresponding to a signal object.
	// We want to be responsive to a shutdown signal (among other events), so we place evt_pipe_sem first in the array
	HANDLE object_handles[] = {thr->evt_pipe_sem, thr->input};
	static const DWORD n_handles = sizeof(object_handles) / sizeof(object_handles[0]);

	char input_key = '\0';
	while (!input_key) {
		LOG(Verbosity_DEBUG, "Blocking input on WaitForMultipleObjects...\n");
		DWORD status = WaitForMultipleObjects(n_handles, object_handles, false, INFINITE);
		LOG(Verbosity_DEBUG, "Done with WaitForMultipleObjects\n");

		// This is verbose but it's better to be API-compliant than rely on UB
		if (!(status >= WAIT_OBJECT_0 && status <= WAIT_OBJECT_0+(n_handles-1))) {
			LOG(Verbosity_NORMAL, "WaitForMultipleObjects failed. This should never happen\n");
			continue;
		}

		const DWORD signal_index = status - WAIT_OBJECT_0;
		// NOTE: semaphores are automatically decremented by WaitForMultipleObjects
		if (signal_index == 0) {
			// Read event from pipe
			TermIO_Event evt;
			bool ok = ReadFile(thr->evt_pipe_rd, &evt, sizeof(TermIO_Event), NULL, NULL);
			if (!ok) {
				LOG(Verbosity_VERBOSE, "Error: evt_pipe_sem desync'd from actual event pipe\n");
				continue;
			}

			switch (evt.event_type) {
				case TermIO_SHUTDOWN:
					return EOF;
				case TermIO_CHANGE_MODE:
					return CONTINUE_IO_LOOP;
				case TermIO_TIMECODE:
					{
						// Update timecode
						strncpy(thr->timecode_buf, evt.body, sizeof(thr->timecode_buf)-1);
						strncpy(thr->duration_buf, evt.body2, sizeof(thr->duration_buf)-1);
						// Render timecode and playback state
						write_playback_info(thr, InputMode_KEY);
					}
					break;

				case TermIO_PLAYBACK_STATE:
					{
						// Update playback state
						thr->playback_state = evt.body_inline;
						// Render timecode and playback state
						write_playback_info(thr, InputMode_KEY);
					}
					break;
			}
			continue;
		}

		DWORD n_read;
		LOG(Verbosity_DEBUG, "Blocking on ReadConsole...\n");
		ReadConsoleA(thr->input, &input_key, sizeof(input_key), &n_read, NULL);
		// We have to flush the input buffer to avoid the keypress event re-firing (yes, it is bad that we have to do this)
		FlushConsoleInputBuffer(thr->input);
		LOG(Verbosity_DEBUG, "Done with ReadConsole\n");
	}

	return input_key;
}

// Enter a readline-powered shell, passing shell line events as they come
static int TermIOThread_shell(TermIOThread *thr) {
}
