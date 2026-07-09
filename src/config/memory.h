#pragma once

#include "parse_v2/types.h"

#include <stdlib.h>
#include <stdint.h>

/* #region Registers */

// A register that holds a ConfigVal
typedef struct ConfigRegister {
	ConfigVal val;
	bool is_owned; // whether the register holds pointer we must free when clearing it
} ConfigRegister;

// Initialize a ConfigRegister to an empty state
void ConfigRegister_init(ConfigRegister *r);

// Clear a register, freeing the data it holds if said data is not owned elsewhere
void ConfigRegister_clear(ConfigRegister *r);

// Store a value in a ConfigRegister (NOTE: this does not clear the register, caller is responsible for doing that beforehand)
void ConfigRegister_store(ConfigRegister *r, ConfigVal val, bool is_owned);

/* #endregion */

/* #region Main memory struct */

// State for variables and registers in MPL's config/shell
typedef struct ConfigMemory {
	ConfigRegister ret; // ConfigFn return value
} ConfigMemory;

// Initialize memory used for config values/registers
void ConfigMemory_init(ConfigMemory *mem);

// Deinitialize all config memory.
// 1. Clear all registers
// 2. Delete all user variables
void ConfigMemory_deinit(ConfigMemory *mem);

/* #endregion */
