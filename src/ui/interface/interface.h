#pragma once
#include "config/config.h"
#include "error.h"
#include "ui/event_queue.h"
#include "track_queue/queue.h"


// Standard interface for things ANY UI for mpl has and can do.
typedef struct UserInterface UserInterface;
struct UserInterface {
	const char *name; // UI name

	EventQueue *evt_queue; // Main event queue owned and handled by the UI. Allows aux threads to message the main thread

	// Initialize UI context (including registering control methods)
	enum UserInterface_ERR (*init)(void *ctx, EventQueue *evt_queue, const Settings *settings);
	// Deinitialize the UI
	void (*deinit)(void *ctx);

	// Run the main UI loop. This always runs on the main thread.
	// This MUST handle events sent on evt_queue
	enum UserInterface_ERR (*mainloop)(void *ctx, EventQueue *evt_queue,
			Queue *track_queue, Config *config);

	// Private UI-specific context
	const size_t ctx_size;
	void *ctx;
};

// Initialize the UI for the main loop to run.
enum UserInterface_ERR UserInterface_init(UserInterface *ui, const Settings *settings);
// Deinitialize the UI for shutdown
void UserInterface_deinit(UserInterface *ui);

// Run the main UI loop
enum UserInterface_ERR UserInterface_mainloop(UserInterface *ui, Queue *track_queue, Config *config);
