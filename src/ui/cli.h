#pragma once
#include "config/config.h"
#include "queue/queue.h"
#include "ui/input_thread.h"

#include <stdint.h>

// MPL's CLI user interface. This is the default UI used
typedef struct UI_CLI {
	EventQueue *evt_queue;
	InputThread *input_thread;
} UI_CLI;

// Initialize CLI user interface and main event queue for use.
// Returns 0 on success, nonzero on error
int UI_CLI_init(UI_CLI *ui);
// Deinitialize CLI user interface for freeing
void UI_CLI_deinit(UI_CLI *ui);

// Handle events until we get mpl_QUIT
int UI_CLI_mainloop(UI_CLI *ui, Queue *queue, Config *config);
