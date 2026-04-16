#include "config/internal/parse.h"
#include "state.h"

/* #region Macro function state  */

static struct macroState state;

void ConfigFn_macroState_init(Config *config) {
	state.config = config;
}

/* #endregion */


void include_default_keybinds(void * _) {
	Config_parse_internal(state.config, NULL, PARSE_KEYBINDS);
}
