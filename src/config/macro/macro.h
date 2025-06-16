#pragma once


// Attempt to parse and evaluate a line of mpl.conf as a config macro (i.e include_default_keybinds()).
// Returns 0 on success, nonzero on error
int macro_eval(const char *line);
