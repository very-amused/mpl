#pragma once
#include <stdint.h>
#include <stdbool.h>

// A crossplatform semaphore type
typedef struct mplSem mplSem;

// A cross platform mutual exclusion lock
typedef struct mplMutex mplMutex;

// A cross platform condvar type
typedef struct mplCondVar mplCondVar;

// Allocate and initialize a new mplSem semaphore to a given value.
// pshared is whether the semaphore will support sharing across *processes* and not just threads (usually false).
// sem_value is the value the semaphore will be initialized to (usually 0)
mplSem *mplSem_malloc(bool pshared, uint32_t sem_value);
// Deinitialize and free a mplSem
void mplSem_free(mplSem *sem);

// Increment an mplSem (nonblocking)
void mplSem_post(mplSem *sem);
// Decrement an mplSem (blocking)
void mplSem_wait(mplSem *sem);

// Allocate and initialize a new mplMutex
mplMutex *mplMutex_malloc();
// Deinitialize and free a mplMutex
void mplMutex_free(mplMutex *lock);

// Lock an mplMutex
void mplMutex_lock(mplMutex *lock);
// Unlock an mplMutex.
// Must be called by the same thread that performed the lock being undone
void mplMutex_unlock(mplMutex *lock);

// Allocate and initialize a new mplCondVar
mplCondVar *mplCondVar_malloc();
// Deinitialize and free a mplCondVar
void mplCondVar_free(mplCondVar *cv);

// Broadcast on *cv, waking up all waiting parties
void mplCondVar_broadcast(mplCondVar *cv);
// Block on *cv, temporarily releasing *lock.
// NOTE: mplCondVar, like the platform-specific CVs it uses under the hood,
// is not immune from spurious wakeups. Always check the CV's associated value on wakeup.
void mplCondVar_wait(mplCondVar *cv, mplMutex *lock);
