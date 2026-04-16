#include "state.h"

/* #region Macro function state  */

static struct macroState state;

void ConfigFn_macroState_init(Config *config) {
	state.config = config;
}

/* #endregion */
