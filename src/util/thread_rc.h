#pragma once
#include <stdbool.h>

// Thread remote control allowing the main thread to
// control an auxilliary cycle-based thread.
typedef struct ThreadRC ThreadRC;

// Methods that the main thread can use to wake an aux thread that has gone to sleep mid-cycle,
// preventing deadlock scenarios.
typedef struct ThreadRC_AntiDeadlock {
	void (*wake_aux_thread)(void *userdata);
} ThreadRC_AntiDeadlock;

/* Main thread methods */

// Allocate and initialize a new ThreadRC for use
ThreadRC *ThreadRC_new(ThreadRC_AntiDeadlock anti_deadlock, void *userdata);
// Deinitialize and free a ThreadRC.
void ThreadRC_free(ThreadRC *rc);

/* Locking a ThreadRC pauses the aux thread in a way that enables recursive locking.
 * This is necessary for things like the ability to seek (which locks a BufferThread)
 * while paused (which also locks the BufferThread) */
void ThreadRC_lock(ThreadRC *rc);
// Unlock a ThreadRC, causing the aux thread to rerun ThreadRC_preloop and then begin its next cycle
// NOTE: ThreadRC_unlock() will return EAGAIN when the thread hasn't actually been unlocked (recursive locking).
int ThreadRC_unlock(ThreadRC *rc);

// Cause the aux thread to break its loop and shutdown
void ThreadRC_shutdown(ThreadRC *rc);

// If the aux thread is in a fail state, recover from it and continue the loop.
// This should be called AFTER the main thread has provided the aux thread with new data.
// This new data should allow the aux thread to recover from any previous fail-state caused by the old data.
//
// Returns whether the ThreadRC was actually in a fail-state.
bool ThreadRC_recover(ThreadRC *rc);

/* Aux thread methods */

// Check and handle ThreadRC state before beginning a loop cycle on the aux thread
// Returns whether the thread should continue (returning false means the thread should break the loop and call pthread_exit)
bool ThreadRC_preloop(ThreadRC *rc);

// Enter a self-locking fail state to prevent an errored thread from spinning indefinitely.
// After calling ThreadRC_selflock(), the aux thread should continue the loop to ThreadRC_preloop(),
// which will then sleep until the main thread calls ThreadRC_recover()
// to tell the aux thread to try resuming.
void ThreadRC_selflock(ThreadRC *rc, int err_code, const char *err_msg);
