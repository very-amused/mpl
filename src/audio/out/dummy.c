#include <pthread.h>

#include "backend.h"
#include "backends.h"
#include "config/settings.h"
#include "audio/track.h"
#include "error.h"
#include "dummy/loop.h"

/* Dummy AudioBackend context, this is basically test state for the AudioBackend APIs */
typedef struct Ctx {
  // Needed to send timecodes and track_end to the main thread
  EventQueue *evt_queue;

  // Asynchronous event loop
  DummyLoop *loop;
} Ctx;

/* Dummy Audiobackend methods */
static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings);
static void deinit(void *ctx__);
static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track);
static enum AudioBackend_ERR play(void *ctx__, bool pause);
static void lock(void *ctx__);
static void unlock(void *ctx__);
static void seek(void *ctx__);

AudioBackend AB_dummy = {
  .name = "dummy (no sound, use for testing)",

  .init = init,
  .deinit = deinit,

  .prepare = prepare,

  .play = play,

  .lock = lock,
  .unlock = unlock,
  .seek = seek,

  .ctx_size = sizeof(Ctx)
};

static enum AudioBackend_ERR init(void *ctx__, const EventQueue *eq, const Settings *settings) {
	return AudioBackend_CONNECT_ERR;
}

static void deinit(void *ctx__) {}

static enum AudioBackend_ERR prepare(void *ctx__, AudioTrack *track) {
	return AudioBackend_CONNECT_ERR;
}

static enum AudioBackend_ERR play(void *ctx__, bool pause) {
	return AudioBackend_CONNECT_ERR;
}

static void lock(void *ctx__) {}

static void unlock(void *ctx__) {}

static void seek(void *ctx__) {}
