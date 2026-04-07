#pragma once
#include "config/settings.h"
#include "error.h"
#include "ui/event_queue.h"

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

	// Initialize the UI for the main loop to run.
	enum UserInterface_ERR (*init)(void *ctx, const Settings *settings);
	// Get control methods that expose UI functionality
	enum UserInterface_ERR (*get_control)(void *ctx);
	// Deinitialize the UI
	void (*deinit)(void *ctx);

	// Run the main UI loop. This always runs on the main thread. This MUST
	// 1. return UserInterface_OK when the mpl_QUIT event is received.
	// 2. Handle keybinds somehow
	enum UserInterface_ERR (*mainloop)(void *ctx);

	// Private UI-specific context
	const size_t ctx_size;
	void *ctx;
};

struct UI_Control {
	enum UserInterface_ERR (*refresh_timecode)(void *ctx); // Refresh or reprint timecode information
	enum UserInterface_ERR (*refresh_metadata)(void *ctx); // Refresh or reprint track metadata
	enum UserInterface_ERR (*show_config)(void *ctx); // Show the user their active MPL config
};
