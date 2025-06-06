#pragma once

#include "error.h"

#include <stdint.h>
#include <wchar.h>

#define CONF_TOKEN_T(VARIANT) \
	VARIANT(confToken_COMMENT) /* A comment: # goes to end of line */ \
	\
	VARIANT(confToken_INT_LIT) /* int32 literal */ \
	VARIANT(confToken_STR_LIT) /* string literal */ \
	\
	VARIANT(confToken_GLOBAL_IDENT) /* The name of a ConfGlobal by mpl */ \
	VARIANT(confToken_FN_IDENT) /* The name of a ConfFn provided by mpl */ \
	VARIANT(confToken_VAR_IDENT) /* The name of a ConfVar */ \
	VARIANT(confToken_KEYBOARD_IDENT) /* Name of a keyboard key */ \
	\
	VARIANT(confToken_PAREN_L) /* ( */ \
	VARIANT(confToken_PAREN_R) /* ) */ \
	\
	VARIANT(confToken_OP_ASSIGN) /* = */ \
	VARIANT(confToken_OP_KEYBIND) /* bind */ \
	VARIANT(confToken_OP_NOT) /* ! */

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

// A configuration token obtained from lexing
typedef struct confToken {
	enum confToken_t token_t;
	union {
		// Identifiers:
		char *ident_name;

		// Literals:
		int32_t int_lit;
		char *str_lit;

		// Keyboard:
		wchar_t keyboard_key;
	} val;
} confToken;

// Deinitialize a confToken, freeing associated memory.
// NOTE: there is no confToken_init(), as there is no zero value for a confToken
void confToken_deinit(confToken *tok);

// A DLL node containing a confToken.
typedef struct confTokenNode confTokenNode;
struct confTokenNode {
	confToken *token;

	confTokenNode *prev, *next; // (flink, blink)
	// if prev == next == this, then this is the sentinel/head node
};

// Initialze the sentinel node for a DLL holding confToken's
void confTokenNode_init_dll(confTokenNode *head);
// Deinitialize a DLL holding confToken's, calling confToken_deinit() on all tokens
void confTokenNode_deinit_dll(confTokenNode *head);

// Tokenize an mpl config string
int confLexer_tokenize(const char *config);
