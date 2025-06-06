#include "conf.h"
#include "error.h"
#include "util/log.h"

#include <stdint.h>
#include <stdio.h>


int echo(uint32_t argc, ConfArg **argv) {
	int status = 0;

	for (uint32_t i = 0; i < argc; i++) {
		const ConfArg *arg = argv[i];
		char delim = i < argc-1 ? ' ' : '\n';
		switch (arg->arg_t) {
		case ConfArg_INT32:
			printf("%d%c", arg->val.i32, delim);
			break;

		case ConfArg_STR:
			printf("%s%c", arg->val.str, delim);
			break;

		case ConfArg_VAR:
			LOG(Verbosity_NORMAL, "ConfArg_VAR is not yet implemented in echo()\n");
			status = 1;
			break;
		}
	}

	return status;
}
