#pragma once

#include "error.h"

#define CONF_TOKEN_T(VARIANT) \
	VARIANT(confToken_COMMENT) /* A comment: # goes to end of line */ \
	VARIANT(confToken_GLOBAL_IDENT) /* The name of a ConfGlobal by mpl */ \
	VARIANT(confToken_FN_IDENT) /* The name of a ConfFn provided by mpl */ \
	VARIANT(confToken_VAR_IDENT) /* The name of a ConfVar */ \
	VARIANT(confToken_PAREN_L) /* ( */ \
	VARIANT(confToken_PAREN_R) /* ) */ \
	VARIANT(confToken_OP_ASSIGN) /* = */ \
	VARIANT(confToken_OP_KEYBIND) /* bind */ \
	VARIANT(confToken_OP_NOT) /* ! */ \
	VARIANT(confToken_INT_LIT) /* int32 literal */ \
	VARIANT(confToken_STR_LIT) /* string literal */ \
	VARIANT(confToken_KEYBIND_KEY) /* Name of a keyboard key */

/* A configuration syntax token */
enum confToken_t {
	CONF_TOKEN_T(ENUM_VAL)
};

static inline const char *confToken_t_name(enum confToken_t token_t) {
	switch (token_t) {
		CONF_TOKEN_T(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef CONF_TOKEN_T


