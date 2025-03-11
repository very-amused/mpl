#include "backend.h"

typedef struct pa_ctx {
} pa_ctx;

static int pa_init(void *ctx__) {
	pa_ctx *ctx = ctx__;

	// TODO: Set up pa_ctx

	return 0;
}

static int pa_deinit(void *ctx__) {
	pa_ctx *ctx = ctx__;

	// TODO: teardown pa_ctx

	return 0;
}

AudioBackend AB_PulseAudio = {
	.name = "PulseAudio",

	.init = pa_init,
	.deinit = pa_deinit,

	.ctx_size = sizeof(pa_ctx)
};
