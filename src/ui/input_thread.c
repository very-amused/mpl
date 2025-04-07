#include "input_thread.h"
#include "event_queue.h"

#include <pthread.h>
#include <semaphore.h>

struct InputThread {
	pthread_t *thread;
	sem_t cancel; // Tell the thread to exit

	// EventQueue for sending input events
	EventQueue *eq;
};
