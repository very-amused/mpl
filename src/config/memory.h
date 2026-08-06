#pragma once

#include "parse_v2/types.h"

#include <stdint.h>

/* #region Registers */

// A register that holds a ConfigVal
// WARN: Do not directly access r->val, use MemRegister_{load,store,clear} routines instead
typedef struct MemRegister {
	const char *name;
	ConfigVal val;
	bool is_owned; // whether the register holds a pointer we must free when clearing it
} MemRegister;

// Initialize a ConfigRegister to an empty state
void MemRegister_init(MemRegister *r, const char *name);

// Clear a register, freeing the data it holds if said data is not owned elsewhere
void MemRegister_clear(MemRegister *r);
// Retrieve the value stored in a ConfigRegister
ConfigVal MemRegister_load(const MemRegister *r);
// Store a value in a ConfigRegister (NOTE: this does not clear the register, caller is responsible for doing that beforehand)
void MemRegister_store(MemRegister *r, ConfigVal val, bool is_owned);

/* #endregion */

/* #region Main memory struct */

// State for variables and registers in MPL's config/shell
typedef struct Memory {
	MemRegister ret; // ConfigFn return value
} Memory;

// Initialize memory used for config values/registers
void Memory_init(Memory *mem);

// Deinitialize all config memory.
// 1. Clear all registers
// 2. Delete all user variables
void Memory_deinit(Memory *mem);

/* #endregion */
