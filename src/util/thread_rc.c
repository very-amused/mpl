#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "thread_rc.h"
#include "error.h"
#include "log.h"

struct ThreadRC_Switches {
	uint32_t lock; // pause the thread's loop, causing it to sleep until !lock
	bool shutdown; // Cause the thread to shutdown, allowing it to be joined with pthread_join
};

struct ThreadRC {
	struct ThreadRC_Switches sw;
	pthread_mutex_t sw_mutex;
	pthread_cond_t sw_wr;
	pthread_cond_t sw_rd;

	struct ThreadRC_AntiDeadlock anti_deadlock;

	void *userdata;
};

ThreadRC *ThreadRC_new(void *userdata, ThreadRC_AntiDeadlock anti_deadlock) {
	ThreadRC *rc = malloc(sizeof(ThreadRC));
	memset(rc, 0, sizeof(ThreadRC));

	pthread_mutex_init(&rc->sw_mutex, NULL);
	pthread_cond_init(&rc->sw_wr, NULL);
	pthread_cond_init(&rc->sw_rd, NULL);

	rc->userdata = userdata;
	rc->anti_deadlock = anti_deadlock;

	return rc;
}

void ThreadRC_free(ThreadRC *rc) {
	free(rc);
}

void ThreadRC_lock(ThreadRC *rc) {
	pthread_mutex_lock(&rc->sw_mutex);

	rc->sw.lock++;

	rc->anti_deadlock.wake_aux_thread(rc->userdata);
	pthread_cond_signal(&rc->sw_wr);

	pthread_cond_wait(&rc->sw_rd, &rc->sw_mutex);
	pthread_mutex_unlock(&rc->sw_mutex);
}

int ThreadRC_unlock(ThreadRC *rc) {
	pthread_mutex_lock(&rc->sw_mutex);

	if (rc->sw.lock == 0) {
		LOG(Verbosity_VERBOSE, "Error: extraneous ThreadRC_unlock() call\n");
		return 1;
	}
	rc->sw.lock--;
	int status = rc->sw.lock > 0 ? EAGAIN : 0;

	rc->anti_deadlock.wake_aux_thread(rc->userdata);
	pthread_cond_signal(&rc->sw_wr);

	pthread_cond_wait(&rc->sw_rd, &rc->sw_mutex);
	pthread_mutex_unlock(&rc->sw_mutex);

	return status;
}


void ThreadRC_preloop(ThreadRC *rc) {
	pthread_mutex_lock(&rc->sw_mutex);

	// Handle switches state
	const struct ThreadRC_Switches *switches = &rc->sw;
	while (switches->lock > 0) {
		pthread_cond_wait(&rc->sw_wr, &rc->sw_mutex);
	}
	if (switches->shutdown) {
		pthread_exit(NULL);
	}

	pthread_cond_signal(&rc->sw_rd);
	pthread_mutex_unlock(&rc->sw_mutex);
}
