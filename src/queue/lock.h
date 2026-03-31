#pragma once

// A platform-independent mutex API used to lock a queue
typedef struct QueueLock QueueLock;

// Allocate and initialize a QueueLock for use. The lock can later be freed using free(3)
// *lock can be NULL when called.
void QueueLock_init(QueueLock **lock);

// Lock a QueueLock, blocking until a lock can be acquired or an error occurs.
// Returns 0 on success, nonzero on error.
int QueueLock_lock(QueueLock *lock);

// Unlock a QueueLock, returning immediately.
// Returns 0 on success, nonzero on error.
// QueueLock_unlock being called by a different thread than the current lock was acquired by is considered an error.
int QueueLock_unlock(QueueLock *lock);
