#pragma once
#include "interface.h"
#include "config/settings.h"

/* Terminal UserInterfaces */
extern UserInterface UI_CommandLine;
// TODO: implement TUI
//extern UserInterface UI_TUI;


/* Get UserInterface set by settings->user_interface,
 * fall back to UI_Default() if unset or invalid */
UserInterface *UI_Configured(const Settings *settings);
/* Get default UserInterface */
UserInterface *UI_Default();
