#include "backend.h"

typedef struct pa_ctx {
} pa_ctx;

static int PulseAudio_init(pa_ctx *ctx) {
	// TODO: Set up pa_ctx

	return 0;
}

AudioBackend AB_PulseAudio = {
	.name = "PulseAudio",

	.ctx_size = sizeof(pa_ctx)
};
