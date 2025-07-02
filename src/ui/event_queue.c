#include "event.h"
#include "event_queue.h"
#include "util/log.h"
#include "error.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __unix__
// Unix only headers
#include <linux/limits.h>
#include <mqueue.h>
#include <unistd.h>
#else
// Windows only headers
#include <Windows.h>
#include <processthreadsapi.h>
#include <stdatomic.h>
#endif

struct EventQueue {
#ifdef __unix__
	// Message queue name, needed to open other handles in the future
	char name[NAME_MAX];
	// Message queue handle
	mqd_t mq; 
#else
	// Main thread ID allows other threads to post messages to the main thread's message queue
	DWORD main_thread_id;
	// Whether our event queue is able to receive messages from other threads
	atomic_bool open;
	// Send/recv permissions
	bool perm_send, perm_recv;
#endif
};

#ifdef __unix__
// Universal EventQueue message queue attributes
static const  struct mq_attr EventQueue_mq_attr = {
	.mq_maxmsg = 5,
	.mq_msgsize = sizeof(Event)
};

static void mq_gen_name_(char *name, size_t name_max) {
	snprintf(name, name_max, "/mpl_event_queue_%d", getpid());
}
#else
// Min value for user-defined Win32 message IDs
static const UINT MSG_ID_BASE = WM_USER;
// Inclusive max for user-defined Win32 message IDs
static const UINT MSG_ID_MAX = WM_APP - 1;

// Get a valid Win32 user-message ID representing an MPL_EVENT message
static UINT MPL_EVENT_w32id(enum MPL_EVENT evt) {
	return MSG_ID_BASE + (UINT)evt;
}
#endif

EventQueue *EventQueue_new() {
	EventQueue *eq = malloc(sizeof(EventQueue));
	eq->open = false;
	eq->perm_recv = eq->perm_send = true;

#ifdef __unix__
	// Generate mq name
	mq_gen_name_(eq->name, sizeof(eq->name));
	eq->mq = mq_open(eq->name, O_RDWR | O_CREAT, 0600, &EventQueue_mq_attr);
	if (eq->mq == -1) {
		perror("EventQueue message queue");
		free(eq);
		return NULL;
	}
#else
	// Get main thread ID
	// (needed to post messages from other threads to this one via PostThreadMessage)
	eq->main_thread_id = GetCurrentThreadId();

	// Tell windows to create a message queue for our main thread
	// (see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postthreadmessagea#remarks)
	MSG w32_msg;
	PeekMessage(&w32_msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	// Open the EventQueue to receive messages from other threads
	atomic_store(&eq->open, true);
#endif

	return eq;
}
void EventQueue_free(EventQueue *eq) {
#ifdef __unix__
	mq_close(eq->mq);
#else
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
#endif
	free(eq);
}

int EventQueue_send(EventQueue *eq, const Event *evt) {
#ifdef __unix__
	int status = mq_send(eq->mq, (const char *)evt, sizeof(Event), 0);
	if (status == -1) {
		perror("EventQueue message send");
		return status;
	}
#else
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
#endif

	return 0;
}
int EventQueue_recv(EventQueue *eq, Event *evt) {
#ifdef __unix__
	int status = mq_receive(eq->mq, (char *)evt, sizeof(Event), 0);
	if (status == -1) {
		perror("EventQueue message receive");
		free(evt);
		return status;
	}
#else
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
#endif

	return 0;
}

EventQueue *EventQueue_connect(const EventQueue *eq1, int oflags) {
	EventQueue *eq = malloc(sizeof(EventQueue));
#ifdef __unix__
	strncpy(eq->name, eq1->name, sizeof(eq->name));
	eq->mq = mq_open(eq->name, oflags, 0600, &EventQueue_mq_attr);
	if (eq->mq == -1) {
		perror("EventQueue_connect message queue");
		free(eq);
		return NULL;
	}
#else
	memcpy(eq, eq1, sizeof(EventQueue));
	if ((oflags & O_RDWR) == O_RDWR) {
		eq->perm_recv = eq->perm_send = true;
	} else {
		eq->perm_recv = (oflags & O_RDONLY) == O_RDONLY;
		eq->perm_send = (oflags & O_WRONLY) == O_WRONLY;
	}
#endif

	return eq;
}
