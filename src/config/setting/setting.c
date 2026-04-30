#include "setting.h"

#include <stdlib.h>

void ConfigSetting_deinit(ConfigSetting *s) {
	free(s->ident);
	s->ident = NULL;
}
