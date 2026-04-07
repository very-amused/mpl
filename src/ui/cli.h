#pragma once
#include "config/config.h"
#include "queue/queue.h"
#include "ui/input_thread.h"

#include <stdint.h>

// MPL's CLI user interface. This is the default UI used
// WARN: deprecated, does not implement UserInterface spec.
typedef struct UI_CLI_legacy {
	EventQueue *evt_queue;
	InputThread *input_thread;
} UI_CLI_legacy;

// Initialize CLI user interface and main event queue for use.
// Returns 0 on success, nonzero on error
int UI_CLI_legacy_init(UI_CLI_legacy *ui);
// Deinitialize CLI user interface for freeing
void UI_CLI_legacy_deinit(UI_CLI_legacy *ui);

// Handle events until we get mpl_QUIT
int UI_CLI_legacy_mainloop(UI_CLI_legacy *ui, Queue *queue, Config *config);
