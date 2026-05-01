#include "function.h"

#include <stdlib.h>
#include <string.h>

void ConfigFn_deinit(ConfigFn *fn) {
	free(fn->ident);
	free(fn->arg_types);
	fn->ident = NULL;
}
