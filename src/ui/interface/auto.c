#include "interfaces.h"
#include "ui/interface/interface.h"
#include "util/log.h"

#include <assert.h>
#include <stdbool.h>

UserInterface *UI_Configured(const Settings *settings) {
	UserInterface *def = UI_Default();
	if (!settings->user_interface) {
		LOG(Verbosity_VERBOSE, "user_interface is unset, using default UserInterface (%s)\n", def->name);
		return def;
	}

	static const char ERR_UNSUPPORTED[] = "user_interface '%s' is unsupported on this build, falling back to default (%s)\n";

	if (strcmp(settings->user_interface, "cli") == 0) {
#if defined(UI_CLI)
		return &UI_CommandLine;
#else
		LOG(Verbosity_NORMAL, ERR_UNSUPPORTED, "cli", def->name);
		return def;
#endif
	}

	return def;
}

UserInterface *UI_Default() {
#ifdef UI_CLI
	return &UI_CommandLine;
#else
	static_assert(false, "Could not find a supported UserInterface.");
#endif
}
