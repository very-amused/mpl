#pragma once
#include "dictionary.h"

/* Register defined settings with a ConfigSettingDict.
 * This is the ONLY step needed to make settings automatically parseable */
void register_ConfigSetting_settings(ConfigSettingDict *dict);
