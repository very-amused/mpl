#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __unix__
#include <pthread.h>
#include <semaphore.h>
#else
// TODO: win32 equivalents
#endif

#include "synchro.h"
#include "util/log.h"

// Log that a section dependent on missing win32 APIs has been reached,
// useful to signal if this has happened in a void function
#define PANIC_NOWIN32() LOG(Verbosity_NORMAL, "Error: missing Win32 implementation!\n"); \
	exit(1)

struct mplSem {
#ifdef __unix__
	sem_t posix_sem;
#else
// TODO: win32 equivalents
#endif
};

struct mplMutex {
#ifdef __unix__
	pthread_mutex_t pthread_mutex;
#else
// TODO: win32 equivalents
#endif
};

struct mplCondVar {
#ifdef __unix__
	pthread_cond_t pthread_cv;
#else
// TODO: win32 equivalents
#endif
};

/* #region mplSem */

mplSem *mplSem_malloc(bool pshared, uint32_t sem_value) {
	mplSem *sem = malloc(sizeof(mplSem));
#ifdef __unix__
	sem_init(&sem->posix_sem, pshared, sem_value);
#else
	PANIC_NOWIN32();
	free(sem);
	sem = NULL;
#endif
	return sem;
}

void mplSem_free(mplSem *sem) {
#ifdef __unix__
	sem_destroy(&sem->posix_sem);
#else
	PANIC_NOWIN32();
#endif
	free(sem);
}

void mplSem_post(mplSem *sem) {
#ifdef __unix__
	sem_post(&sem->posix_sem);
#else
	PANIC_NOWIN32();
#endif
}

void mplSem_wait(mplSem *sem) {
#ifdef __unix__
	sem_wait(&sem->posix_sem);
#else
	PANIC_NOWIN32();
#endif
}

/* #endregion */

/* #region mplMutex */

mplMutex *mplMutex_malloc() {
	mplMutex *lock = malloc(sizeof(mplMutex));
#ifdef __unix__
	pthread_mutex_init(&lock->pthread_mutex, NULL);
#else
	PANIC_NOWIN32();
	free(lock);
	lock = NULL;
#endif
	return lock;
}

void mplMutex_free(mplMutex *lock) {
#ifdef __unix__
	pthread_mutex_destroy(&lock->pthread_mutex);
#else
	PANIC_NOWIN32();
#endif
	free(lock);
}

void mplMutex_lock(mplMutex *lock) {
#ifdef __unix__
	pthread_mutex_lock(&lock->pthread_mutex);
#else
	PANIC_NOWIN32();
#endif
}

void mplMutex_unlock(mplMutex *lock) {
#ifdef __unix
	pthread_mutex_lock(&lock->pthread_mutex);
#else
	PANIC_NOWIN32();
#endif
}

/* #endregion */

/* #region mplCondVar */


mplCondVar *mplCondVar_malloc() {
	mplCondVar *cv = malloc(sizeof(mplCondVar));
#ifdef __unix__
	pthread_cond_init(&cv->pthread_cv, NULL);
#else
	PANIC_NOWIN32();
	free(cv);
	cv = NULL;
#endif
	return cv;
}

void mplCondVar_free(mplCondVar *cv) {
#ifdef __unix__
	pthread_cond_destroy(&cv->pthread_cv);
#else
	PANIC_NOWIN32();
#endif
	free(cv);
}

void mplCondVar_broadcast(mplCondVar *cv) {
#ifdef __unix__
	pthread_cond_broadcast(&cv->pthread_cv);
#else
	PANIC_NOWIN32();
#endif
}

void mplCondVar_wait(mplCondVar *cv, mplMutex *lock) {
#ifdef __unix__
	pthread_cond_wait(&cv->pthread_cv, &lock->pthread_mutex);
#else
	PANIC_NOWIN32();
#endif
}

/* #endregion */
