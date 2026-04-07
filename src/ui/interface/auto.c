#include "interfaces.h"
#include "ui/interface/interface.h"

#include <assert.h>
#include <stdbool.h>

UserInterface *UI_Configured(const Settings *settings) {
	UserInterface *def = UI_Default();

	// TODO: implement user_interface setting
	return def;
}

UserInterface *UI_Default() {
#ifdef UI_CLI
	return &UI_CommandLine;
#else
	static_assert(false, "Could not find a supported UserInterface.");
#endif
}
