#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

#include <errhandlingapi.h>
#include <fcntl.h>
#include <stdint.h>
#include <windows.h>
#include <initguid.h>
#include <combaseapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <ksmedia.h>
#include <winerror.h>
#include <handleapi.h>

#include "wasapi_fb_thread.h"
#include "util/log.h"

struct WASAPI_fbThread {
	pthread_t *thread;
	pthread_mutex_t lock; // lock over thread resources
 
	HANDLE write_evt; // Tell the thread to call write_callback
	HANDLE cancel_evt; // Tell the thread to exit

	WASAPI_write_cb_t write_callback;
	void *userdata; // userdata passed to callback
};


WASAPI_fbThread *WASAPI_fbThread_new() {
	WASAPI_fbThread *thr = malloc(sizeof(WASAPI_fbThread));
	if (!thr) {
		return NULL;
	}
	memset(thr, 0, sizeof(*thr));

	// Initialize events
	thr->write_evt = CreateEventA(NULL, false, false, NULL);
	if (!thr->write_evt) {
		WASAPI_fbThread_free(thr);
		return NULL;
	}
	thr->cancel_evt = CreateEventA(NULL, true, false, NULL);
	if (!thr->cancel_evt) {
		WASAPI_fbThread_free(thr);
		return NULL;
	}

	// Initialize locks
	pthread_mutex_init(&thr->lock, NULL);

	return thr;
}

void WASAPI_fbThread_free(WASAPI_fbThread *thr) {
	if (thr->thread) {
		pthread_mutex_lock(&thr->lock);
		SetEvent(thr->cancel_evt);
		pthread_mutex_unlock(&thr->lock);
		pthread_join(*thr->thread, NULL);
	}
	free(thr->thread);

	if (thr->write_evt) {
		CloseHandle(thr->write_evt);
	}
	if (thr->cancel_evt) {
		CloseHandle(thr->cancel_evt);
	}

	free(thr);
}

void WASAPI_fbThread_lock(WASAPI_fbThread *thr) {
	pthread_mutex_lock(&thr->lock);
}
void WASAPI_fbThread_unlock(WASAPI_fbThread *thr) {
	pthread_mutex_unlock(&thr->lock);
}

HANDLE WASAPI_fbThread_get_write_evt_handle(WASAPI_fbThread *thr) {
	return thr->write_evt;
}

void WASAPI_fbThread_set_write_cb(WASAPI_fbThread *thr, WASAPI_write_cb_t cb, void *userdata) {
	thr->write_callback = cb;
	thr->userdata = userdata;
}

static void *WASAPI_fbThread_routine(void *args) {
	WASAPI_fbThread *thr = args;
	HANDLE object_handles[] = {thr->cancel_evt, thr->write_evt};
	static const DWORD n_handles = sizeof(object_handles) / sizeof(object_handles[0]);

	while (true) {
		WASAPI_fbThread_lock(thr);

		// See input_thread_win32.c for remarks on these routines
		DWORD status = WaitForMultipleObjects(n_handles, object_handles, false, INFINITE);
		if (!(status >= WAIT_OBJECT_0 && status <= WAIT_OBJECT_0+(n_handles-1))) {
			LOG(Verbosity_NORMAL, "WaitForMultipleObjects failed. This should never happen\n");
			WASAPI_fbThread_unlock(thr);
			continue;
		}

		const DWORD signal_index = status - WAIT_OBJECT_0;
		if (signal_index == 0) {
			LOG(Verbosity_DEBUG, "Got shutdown signal in WASAPI_fbThread_routine\n");
			break;
		}

		// Call write callback with lock
		thr->write_callback(thr->userdata);

		WASAPI_fbThread_unlock(thr);
	}

	return NULL;
}

int WASAPI_fbThread_start(WASAPI_fbThread *thr) {
	thr->thread = malloc(sizeof(pthread_t));
	return pthread_create(thr->thread, NULL, WASAPI_fbThread_routine, thr);
}
