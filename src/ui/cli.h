#pragma once
#include "ui/input_thread.h"


// MPL's CLI user interface. This is the default UI used
typedef struct UserInterface_CLI {
	EventQueue *evt_queue;
	InputThread *input_thread;
} UserInterface_CLI;

// Initialize CLI user interface for use.
// Returns 0 on success, nonzero on error
int UserInterface_CLI_init(UserInterface_CLI *ui);
// Deinitialize CLI user interface for freeing
void UserInterface_CLI_deinit(UserInterface_CLI *ui);
