#pragma once
#ifdef AO_WASAPI
#include <windows.h>
#include <initguid.h>
#include <audioclient.h>

// WASAPI framebuffer thread
typedef struct WASAPI_fbThread WASAPI_fbThread;
// Callback for whenever we need to write to WASAPI's framebuffer
typedef void (*WASAPI_write_cb_t)(void *userdata);

// Allocate and initialize a new WASAPI_fbThread
// Returns NULL on error
WASAPI_fbThread *WASAPI_fbThread_new();
// Stop, deinitialize and free a WASAPI_fbThread
void WASAPI_fbThread_free(WASAPI_fbThread *thr);

// Lock framebuffer thread loop
// NOTE: the write callback is automatically called with this lock held
void WASAPI_fbThread_lock(WASAPI_fbThread *thr);
// Unlock framebuffer thread loop
void WASAPI_fbThread_unlock(WASAPI_fbThread *thr);

// Get event handle to register with WASAPI
HANDLE WASAPI_fbThread_get_write_evt_handle(WASAPI_fbThread *thr);
// Set write callback
void WASAPI_fbThread_set_write_cb(WASAPI_fbThread *thr, WASAPI_write_cb_t cb, void *userdata);

// Start framebuffer loop
// NOTE: caller never needs to pause this loop, WASAPI does that automatically
// Returns 0 on success, nonzero on error
int WASAPI_fbThread_start(WASAPI_fbThread *thr);
#endif
