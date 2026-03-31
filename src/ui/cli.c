#include "cli.h"
#include "ui/event_queue.h"
#include "ui/input_thread.h"
#include "util/log.h"
#include <string.h>

int UserInterface_CLI_init(UserInterface_CLI *ui, EventQueue *evt_queue) {
	// Zero pointers so we can use deinit as an escape hatch
	memset(ui, 0, sizeof(UserInterface_CLI));

	// Initialize input thread
	ui->input_thread = InputThread_new(evt_queue);
	if (!ui->input_thread) {
		UserInterface_CLI_deinit(ui);
		return 1;
	}

	return 0;
}

void UserInterface_CLI_deinit(UserInterface_CLI *ui) {
	InputThread_free(ui->input_thread);
	LOG(Verbosity_DEBUG, "InputThread_free finished\n");
}
