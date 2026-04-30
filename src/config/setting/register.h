#pragma once
#include "dictionary.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Register defined settings with a ConfigSettingDict.
 * This is the ONLY step needed to make settings automatically parseable */
void register_ConfigSetting_settings(ConfigSettingDict *dict);

#ifdef __cplusplus
}
#endif
