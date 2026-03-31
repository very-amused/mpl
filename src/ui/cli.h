#pragma once
#include "config/config.h"
#include "queue/queue.h"
#include "ui/input_thread.h"

#include <stdint.h>

// MPL's CLI user interface. This is the default UI used
typedef struct UserInterface_CLI {
	EventQueue *evt_queue;
	InputThread *input_thread;
} UserInterface_CLI;

// Initialize CLI user interface and main event queue for use.
// Returns 0 on success, nonzero on error
int UserInterface_CLI_init(UserInterface_CLI *ui);
// Deinitialize CLI user interface for freeing
void UserInterface_CLI_deinit(UserInterface_CLI *ui);

// Handle events until we get mpl_QUIT
int UserInterface_CLI_mainloop(UserInterface_CLI *ui, Queue *queue, Config *config);
