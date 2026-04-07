#pragma once
#include "config/config.h"
#include "error.h"
#include "ui/event_queue.h"
#include "queue/queue.h"

// Universal UI control methods.
// Used for keybinds to mutate UI state.
//
// WARN: None of these methods are guranteed to be non-NULL,
// keybind functions MUST check these each time before calling.
typedef struct UI_Control UI_Control;

// Standard interface for things ANY UI for mpl has and can do.
typedef struct UserInterface UserInterface;
struct UserInterface {
	const char *name; // UI name
	const UI_Control *ctrl; // UI control methods

	EventQueue *evt_queue; // Main event queue owned and handled by the UI. Allows aux threads to message the main thread

	// Initialize UI context (including registering control methods)
	enum UserInterface_ERR (*init)(void *ctx, EventQueue *evt_queue, const Settings *settings);
	// Deinitialize the UI
	void (*deinit)(void *ctx);

	// Run the main UI loop. This always runs on the main thread. This MUST
	// 1. return UserInterface_OK when the mpl_QUIT event is received.
	// 2. Handle keybinds somehow
	enum UserInterface_ERR (*mainloop)(void *ctx, EventQueue *evt_queue,
			Queue *track_queue, Config *config);

	// Private UI-specific context
	const size_t ctx_size;
	void *ctx;
};

struct UI_Control {
	enum UserInterface_ERR (*refresh_timecode)(void *ctx); // Refresh or reprint timecode information
	enum UserInterface_ERR (*refresh_metadata)(void *ctx); // Refresh or reprint track metadata
	enum UserInterface_ERR (*show_config)(void *ctx); // Show the user their active MPL config
};

// Initialize the UI for the main loop to run.
enum UserInterface_ERR UserInterface_init(UserInterface *ui, const Settings *settings);
// Deinitialize the UI for shutdown
void UserInterface_deinit(UserInterface *ui);

// Run the main UI loop
enum UserInterface_ERR UserInterface_mainloop(UserInterface *ui, Queue *track_queue, Config *config);
