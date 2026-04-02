#pragma once

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
ThreadRC *ThreadRC_new(void *userdata, ThreadRC_AntiDeadlock anti_deadlock);
// Deinitialize and free a ThreadRC.
void ThreadRC_free(ThreadRC *rc);

/* Locking a ThreadRC pauses the aux thread with a semaphore to enable recursive locking.
 * This is necessary for things like the ability to seek (which locks a BufferThread)
 * while paused (which also locks the BufferThread) */
void ThreadRC_lock(ThreadRC *rc);
// Unlock a ThreadRC, causing the aux thread to rerun ThreadRC_preloop and then begin its next cycle
// Recursive locking is enabled through an internal semaphore.
// NOTE: ThreadRC_unlock() will return EAGAIN when the thread hasn't actually been unlocked.
int ThreadRC_unlock(ThreadRC *rc);


/* Aux thread methods */

// Check and handle ThreadRC state before beginning a loop cycle on the aux thread
void ThreadRC_preloop(ThreadRC *rc);
