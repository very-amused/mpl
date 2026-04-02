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
	pthread_mutex_t mutex;
	pthread_cond_t wr;
	pthread_cond_t rd;

	uint32_t thread_lock; // pause the thread's loop, causing it to sleep until !lock
	bool thread_shutdown; // Cause the thread to shutdown, allowing it to be joined with pthread_join

	/* Fail state recovery, NOT settable from the main thread other than using ThreadRC_recover() to check and clear selflock. */
	bool thread_selflock_;
	int thread_selflock_err_code_;
	char *thread_selflock_err_msg_;
};

void ThreadRC_Switches_init(struct ThreadRC_Switches *sw) {
	memset(sw, 0, sizeof(struct ThreadRC_Switches));
	
	// Initialize synchronization primitives
	pthread_mutex_init(&sw->mutex, NULL);
	pthread_cond_init(&sw->wr, NULL);
	pthread_cond_init(&sw->rd, NULL);
}

struct ThreadRC {
	struct ThreadRC_Switches sw; // remote control and fail state recovery
	struct ThreadRC_AntiDeadlock anti_deadlock; // deadlock prevention

	void *userdata;
};

ThreadRC *ThreadRC_new(ThreadRC_AntiDeadlock anti_deadlock, void *userdata) {
	ThreadRC *rc = malloc(sizeof(ThreadRC));
	memset(rc, 0, sizeof(ThreadRC));

	ThreadRC_Switches_init(&rc->sw);

	rc->anti_deadlock = anti_deadlock;
	rc->userdata = userdata;

	return rc;
}

void ThreadRC_free(ThreadRC *rc) {
	free(rc);
}

void ThreadRC_lock(ThreadRC *rc) {
	pthread_mutex_lock(&rc->sw.mutex);

	rc->sw.thread_lock++;

	rc->anti_deadlock.wake_aux_thread(rc->userdata);
	pthread_cond_signal(&rc->sw.wr);

	pthread_cond_wait(&rc->sw.rd, &rc->sw.mutex);
	pthread_mutex_unlock(&rc->sw.mutex);
}

int ThreadRC_unlock(ThreadRC *rc) {
	pthread_mutex_lock(&rc->sw.mutex);

	if (rc->sw.thread_lock == 0) {
		LOG(Verbosity_VERBOSE, "Error: extraneous ThreadRC_unlock() call\n");
		return 1;
	}
	rc->sw.thread_lock--;
	int status = rc->sw.thread_lock > 0 ? EAGAIN : 0;

	rc->anti_deadlock.wake_aux_thread(rc->userdata);
	pthread_cond_signal(&rc->sw.wr);

	pthread_cond_wait(&rc->sw.rd, &rc->sw.mutex);
	pthread_mutex_unlock(&rc->sw.mutex);

	return status;
}

void ThreadRC_shutdown(ThreadRC *rc) {
	pthread_mutex_lock(&rc->sw.mutex);

	if (rc->sw.thread_lock > 0) {
		LOG(Verbosity_VERBOSE, "Error: ThreadRC_shutdown() called on locked ThreadRC\n");
	}

	rc->sw.thread_shutdown = true;

	rc->anti_deadlock.wake_aux_thread(rc->userdata);
	pthread_cond_signal(&rc->sw.wr);

	pthread_mutex_unlock(&rc->sw.mutex);
}

bool ThreadRC_recover(ThreadRC *rc) {
	pthread_mutex_lock(&rc->sw.mutex);

	const bool had_selflock = rc->sw.thread_selflock_;
	if (had_selflock) {
		if (rc->sw.thread_selflock_err_msg_) {
			LOG(Verbosity_DEBUG, "Recovering from ThreadRC selflock. Selflock error: %s (code %d)\n",
					rc->sw.thread_selflock_err_msg_, rc->sw.thread_selflock_err_code_);
		} else {
			LOG(Verbosity_DEBUG, "Recovering from ThreadRC selflock. Selflock error code: %d)\n",
					rc->sw.thread_selflock_err_code_);
		}
	}
	rc->sw.thread_selflock_ = false;
	rc->sw.thread_selflock_err_code_ = 0;
	free(rc->sw.thread_selflock_err_msg_);

	rc->anti_deadlock.wake_aux_thread(rc->userdata);
	pthread_cond_signal(&rc->sw.wr);

	pthread_cond_wait(&rc->sw.rd, &rc->sw.mutex);
	pthread_mutex_unlock(&rc->sw.mutex);

	return had_selflock;
}

/* Aux thread methods */

bool ThreadRC_preloop(ThreadRC *rc) {
	pthread_mutex_lock(&rc->sw.mutex);

	// Handle switches state
	const struct ThreadRC_Switches *switches = &rc->sw;
check_switches:
	pthread_cond_signal(&rc->sw.rd);
	if (switches->thread_shutdown) {
		pthread_mutex_unlock(&rc->sw.mutex);
		return false; // break aux thread loop
	}
	if (switches->thread_lock > 0 || switches->thread_selflock_) {
		pthread_cond_wait(&rc->sw.wr, &rc->sw.mutex);
		goto check_switches;
	}

	pthread_mutex_unlock(&rc->sw.mutex);
	return true; // continue aux thread loop
}

void ThreadRC_selflock(ThreadRC *rc, int err_code, const char *err_msg) {
	pthread_mutex_lock(&rc->sw.mutex);
	
	rc->sw.thread_selflock_ = true;
	rc->sw.thread_selflock_err_code_ = err_code;
	rc->sw.thread_selflock_err_msg_ = strdup(err_msg);

	pthread_mutex_unlock(&rc->sw.mutex);
}
