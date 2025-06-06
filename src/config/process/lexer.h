#pragma once

#include "error.h"

#define CONF_TOKEN(VARIANT) \
	VARIANT(confToken_COMMENT) \
	VARIANT(confToken_GLOBAL_IDENT) \
	VARIANT(confToken_FN_IDENT) \
	VARIANT(confToken_VAR_IDENT) \
	VARIANT(confToken_PAREN_L) \
	VARIANT(confToken_PAREN_R) \
	VARIANT(confToken_OP_ASSIGN) \
	VARIANT(confToken_OP_KEYBIND) \
	VARIANT(confToken_OP_NOT) \
	VARIANT(confToken_INT_LIT) \
	VARIANT(confToken_STR_LIT) \
	VARIANT(confToken_KEY_COMB)
