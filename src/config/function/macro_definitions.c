#include "config/parse_v2/parser.h"
#include "state.h"

/* #region Macro function state  */

static struct macroState state;

void ConfigFn_macroState_init(Config *config) {
	state.config = config;
}

/* #endregion */


void include_default_keybinds(void * _) {
	Parser_walk(state.config->parser, state.config, Parser_WALK_KEYBINDS, state.config->defaults);
}
