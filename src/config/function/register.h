#pragma once

#include "dictionary.h"

/* Register defined functions with a ConfigFnDict.
 * This is the ONLY step needed to make config functions automatically parseable and callable */
void register_ConfigFn_functions(ConfigFnDict *dict);
/* Register defined macros with a ConfigFnDict.
 * This is the ONLY step needed to make config macros automatically parseable and callable */
void register_ConfigFn_macros(ConfigFnDict *dict);
