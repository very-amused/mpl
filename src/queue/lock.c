#include "lock.h"
#include <pthread.h>
#include <stdlib.h>

struct QueueLock {
	pthread_mutex_t mutex;
};

void QueueLock_init(QueueLock **lock) {
	*lock = malloc(sizeof(QueueLock));
	pthread_mutex_init(&(*lock)->mutex, NULL);
}

int QueueLock_lock(QueueLock *lock) {
	return pthread_mutex_lock(&lock->mutex);
}
int QueueLock_unlock(QueueLock *lock) {
	return pthread_mutex_unlock(&lock->mutex);
}
