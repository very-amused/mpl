#pragma once
/* This file defines the basic conf-globals, conf-functions, and conf-variables
 * that can be read and/or manipulated in mpl.conf
 */

#include "error.h"

#include <stdint.h>


/* conf-functions: functions callable from mpl.conf.
 * Every mplConfFn:
 * 1. Takes two args: uint32_t argc and (ConfArg **)argv.
 * An mplConfArg is a tagged union that can contain a signed 32-bit integer, a string, or the name of a mplConfVar.
 * 2. Returns an int, which denotes whether the function call was successful (0), resulted in a warning state (>0), or resulted in an error state (<0).
 */

/* An argument passed to a ConfFn */
typedef struct ConfArg {
	enum ConfArg_t arg_t;
	union {
		int32_t i32;
		char *str;
		char *var_name; /* The name of a ConfVar */
	} val;
} ConfArg;

/* A function callable in mpl.conf */
typedef int (*ConfFn)(uint32_t argc, ConfArg **argv);

/* # README: MPL ConfFn's: */
// Prints its argument(s) to stdout
int echo(uint32_t argc, ConfArg **argv);
