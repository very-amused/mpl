#include "interface.h"
#include "error.h"
#include "ui/event_queue.h"

#include <string.h>


enum UserInterface_ERR UserInterface_init(UserInterface *ui, const Settings *settings) {
	// Initialize event queue
	ui->evt_queue = EventQueue_new();
	if (!ui->evt_queue) {
		return UserInterface_BAD_ALLOC;
	}

	// Allocate UI context
	if (ui->ctx_size) {
		ui->ctx = malloc(ui->ctx_size);
		CHECK_ALLOC(ui->ctx, UserInterface_BAD_ALLOC);
	}

	return ui->init(ui->ctx, ui->evt_queue, settings);
}

void UserInterface_deinit(UserInterface *ui) {
	if (ui->ctx) {
		ui->deinit(ui->ctx);
	}
	free(ui->ctx);
	ui->ctx = NULL;

	if (ui->evt_queue) {
		EventQueue_free(ui->evt_queue);
		ui->evt_queue = NULL;
	}
}

enum UserInterface_ERR UserInterface_mainloop(UserInterface *ui, Queue *track_queue, Config *config) {
	return ui->mainloop(ui->ctx, ui->evt_queue,
			track_queue, config);
}
