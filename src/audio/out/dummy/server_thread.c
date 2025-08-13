#include "server_thread.h"
#include "audio/buffer.h"
#include "audio/pcm.h"
#include "config/settings.h"

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

static void *DummyServerThread_routine(void *args) {
	DummyServerThread *thr = args;

	// For all common sample rates to work as expected, we need to compute an exact number of bytes to read every 10ms
	//
	// The is bc of rates such as 44.1khz: we can read exactly 441 samples per channel each 10ms, but we can't read exactly 44.1 every 1ms
	const AudioPCM *pcm = thr->pcm;
	// TODO: need AudioPCM method
	// uint64_t samples_per_sec = pcm->sample_rate * AudioPCM_

	// Start reading from thr->buffer
	// TODO
}

int DummyServerThread_init(DummyServerThread *thr, const AudioPCM *pcm, const Settings *settings) {
	thr->buffer = malloc(sizeof(AudioBuffer));
	thr->pcm = pcm;
	thr->settings = settings;
	if (AudioBuffer_init(thr->buffer, thr->pcm, thr->settings) != 0) {
		DummyServerThread_deinit(thr);
		return 1;
	}



	return 0;
}

void DummyServerThread_deinit(DummyServerThread *thr) {
	// TODO
}
