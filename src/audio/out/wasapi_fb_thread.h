#pragma once
#ifdef AO_WASAPI
#include <windows.h>
#include <initguid.h>
#include <audioclient.h>

// WASAPI framebuffer thread
typedef struct WASAPI_FB_Thread WASAPI_FB_Thread;
// Callback for whenever we need to write to WASAPI's framebuffer
typedef void (*WASAPI_write_cb_t)(void *userdata);

// Allocate and initialize a new WASAPI_FB_Thread
// Returns NULL on error
WASAPI_FB_Thread *WASAPI_FB_Thread_new();
// Stop, deinitialize and free a WASAPI_FB_Thread
void WASAPI_FB_Thread_free(WASAPI_FB_Thread *thr);

// Lock framebuffer thread loop
// NOTE: the write callback is automatically called with this lock held
void WASAPI_FB_Thread_lock(WASAPI_FB_Thread *thr);
// Unlock framebuffer thread loop
void WASAPI_FB_Thread_unlock(WASAPI_FB_Thread *thr);

// Set write callback
void WASAPI_FB_Thread_set_write_cb(WASAPI_FB_Thread *thr, WASAPI_write_cb_t cb, void *userdata);

// Start framebuffer loop
// NOTE: caller never needs to pause this loop, WASAPI does that automatically
// Returns 0 on success, nonzero on error
int WASAPI_FB_Thread_start(WASAPI_FB_Thread *thr);
#endif
