#pragma once
#include <stdint.h>

#include "audio/pcm.h"
#include "audio/buffer.h"

// Dummy audio server that consumes audio frames from an AudioBuffer on a fixed clock
// A DummyLoop is responsible for starting and stopping this server,
// as well as writing audio frames to the server's buffer
typedef struct DummyServer DummyServer;

// Allocate and initialize a DummyServer.
// This starts the server, which will consume frames until stopped via [DummyServer_free].
//
// Parameters control creation of the server's audio buffer.
DummyServer *DummyServer_new(const AudioPCM pcm, uint32_t buf_ms);
// Stop, deinitialize, and free a DummyServer.
// Any DummyLoop communicating with the server should be stopped before calling.
void DummyServer_free(DummyServer *server);

// Get a reference to the server's AudioBuffer with write-ownership.
// This is used by DummyLoop's to write audio frames.
AudioBuffer *DummyServer_get_buffer(DummyServer *server);

// Reset a DummyServer's audio buffer to hold a given number of ms of PCM data
// Returns 0 on success, nonzero on error
int DummyServer_reset_buffer(const AudioPCM pcm, uint32_t buf_ms);
