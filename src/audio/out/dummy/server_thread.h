#pragma once
#include <pthread.h>

#include "audio/buffer.h"
#include "audio/pcm.h"
#include "config/settings.h"

typedef struct DummyServerThread {
	pthread_t thread;

	AudioBuffer *buffer; // internal server buffer
	const AudioPCM *pcm;
	const Settings *settings;
} DummyServerThread;

// Initialize and start a dummy audio server
int DummyServerThread_init(DummyServerThread *thr, const AudioPCM *pcm, const Settings *settings);
// Stop and deinitialize a dummy audio server
void DummyServerThread_deinit(DummyServerThread *thr);
