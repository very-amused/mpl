#include "cli.h"
#include "ui/event_queue.h"
#include "ui/input_thread.h"
#include <string.h>

int UserInterface_CLI_init(UserInterface_CLI *ui) {
	// Zero pointers so we can use deinit as an escape hatch
	memset(ui, 0, sizeof(UserInterface_CLI));

	// Initialize event queue
	ui->evt_queue = EventQueue_new();
	if (!ui->evt_queue) {
		UserInterface_CLI_deinit(ui);
		return 1;
	}
	// Initialize input thread
	ui->input_thread = InputThread_new(ui->evt_queue);
	if (!ui->input_thread) {
		UserInterface_CLI_deinit(ui);
		return 1;
	}

	return 0;
}

void UserInterface_CLI_deinit(UserInterface_CLI *ui) {
	InputThread_free(ui->input_thread);
	EventQueue_free(ui->evt_queue);
}
