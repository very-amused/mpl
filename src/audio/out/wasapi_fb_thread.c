#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>

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

#include "wasapi_fb_thread.h"

struct WASAPI_FB_Thread {
	pthread_t *thread;
	pthread_mutex_t thread_lock; // lock over thread resources
 
	HANDLE wake_evt; // Tell the thread to call write_callback
	HANDLE cancel_evt; // Tell the thread to exit

	WASAPI_write_cb_t write_callback;
	void *userdata; // userdata passed to callback
};


