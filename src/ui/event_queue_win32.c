#include "event.h"
#include "event_queue.h"
#include "util/log.h"
#include "error.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>

// Windows only headers
#include <Windows.h>
#include <processthreadsapi.h>
#include <stdatomic.h>

struct EventQueue {
	// Main thread ID allows other threads to post messages to the main thread's message queue
	DWORD main_thread_id;
	// Whether our event queue is able to receive messages from other threads
	atomic_bool open;
	// Send/recv permissions
	bool perm_send, perm_recv;
};

// Min value for user-defined Win32 message IDs
static const UINT MSG_ID_BASE = WM_USER;
// Inclusive max for user-defined Win32 message IDs
static const UINT MSG_ID_MAX = WM_APP - 1;

// Get a valid Win32 user-message ID representing an MPL_EVENT message
static UINT MPL_EVENT_w32id(enum MPL_EVENT evt) {
	return MSG_ID_BASE + (UINT)evt;
}

EventQueue *EventQueue_new() {
	EventQueue *eq = malloc(sizeof(EventQueue));

	eq->open = false;
	eq->perm_recv = eq->perm_send = true;

	// Get main thread ID
	// (needed to post messages from other threads to this one via PostThreadMessage)
	eq->main_thread_id = GetCurrentThreadId();

	// Tell windows to create a message queue for our main thread
	// (see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postthreadmessagea#remarks)
	MSG w32_msg;
	PeekMessage(&w32_msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	// Open the EventQueue to receive messages from other threads
	atomic_store(&eq->open, true);

	return eq;
}

void EventQueue_free(EventQueue *eq) {
	atomic_store(&eq->open, false);
	// Drain all posted messages in our queue and free their associated memory
	MSG w32_msg;
	while (PeekMessage(&w32_msg, NULL, MSG_ID_BASE, MSG_ID_MAX, PM_REMOVE)) {
		Event *evt = (Event *)w32_msg.lParam;
		if (evt->body_size > sizeof(evt->body_inline)) {
			free(evt->body);
		}
		free(evt);
	}
	free(eq);
}

int EventQueue_send(EventQueue *eq, const Event *evt) {
	if (!atomic_load(&eq->open) || !eq->perm_send) {
		return -1; // closed
	}
	// Copy event to heap so it remains valid in the queue
	Event *evt2 = malloc(sizeof(Event));
	memcpy(evt2, evt, sizeof(Event));

	// Post message to the main thread
	bool ok = PostThreadMessage(eq->main_thread_id, MPL_EVENT_w32id(evt2->event_type), 0, (LPARAM)evt2);
	if (!ok) {
		LOG(Verbosity_NORMAL, "EventQueue message send fail (%s)\n", MPL_EVENT_name(evt->event_type));
		return 1;
	}

	return 0;
}

int EventQueue_recv(EventQueue *eq, Event *evt) {
	if (!eq->perm_recv) {
		return -1;
	}
	MSG w32_msg;
	int status = GetMessage(&w32_msg, (HWND)-1, MSG_ID_BASE, MSG_ID_MAX);
	if (status == -1) {
		w32_perror(L"EventQueue message receive");
		return status;
	} else if (status == 0) {
		LOG(Verbosity_NORMAL, "EventQueue message receive: got WM_QUIT, relaying as %s\n", MPL_EVENT_name(mpl_QUIT));
		Event close_evt = {.event_type = mpl_QUIT, .body_size = 0};
		return EventQueue_send(eq, &close_evt);
	}

	memcpy(evt, (Event *)w32_msg.lParam, sizeof(Event));

	return 0;
}

EventQueue *EventQueue_connect(const EventQueue *eq1, int oflags) {
	EventQueue *eq = malloc(sizeof(EventQueue));

	memcpy(eq, eq1, sizeof(EventQueue));
	if ((oflags & O_RDWR) == O_RDWR) {
		eq->perm_recv = eq->perm_send = true;
	} else {
		eq->perm_recv = (oflags & O_RDONLY) == O_RDONLY;
		eq->perm_send = (oflags & O_WRONLY) == O_WRONLY;
	}

	return eq;
}
