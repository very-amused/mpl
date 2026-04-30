#pragma once
#include "error.h"

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#define TOKEN_ENUM(VARIANT) \
	/* Symbols */ \
	VARIANT(Tok_LF) /* Newline */ \
	VARIANT(Tok_Semi) /* ; */ \
	VARIANT(Tok_EQ) /* = */ \
	VARIANT(Tok_Lparen) /* ( */ \
	VARIANT(Tok_Rparen) /* ) */ \
	VARIANT(Tok_Comma) /* , */ \
	VARIANT(Tok_Bind) /* bind (used in keybind definitions) */ \
	/* Literals */ \
	VARIANT(Tok_I32Lit) \
	VARIANT(Tok_StrLit) \
	VARIANT(Tok_BoolLit) \
	/* Identifiers */ \
	VARIANT(Tok_Ident) /* Name of a MPL setting/function/something */ \
	/* Key names */ \
	VARIANT(Tok_Keysym) /* Keyboard symbol to be used in a keybind */

enum LexerToken_t {
	TOKEN_ENUM(ENUM_VAL)
};

static inline const char *LexerToken_t_name(enum LexerToken_t tok) {
	switch (tok) {
		TOKEN_ENUM(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef TOKEN_ENUM

// A logical token used in parsing
typedef struct LexerToken {
	enum LexerToken_t type;
	union {
		int32_t i32_lit;
		char *str_lit;
		bool bool_lit;

		// Identifier for something
		char *ident;

		// keycode identifying a keyboard key
		wchar_t keycode;
	};
} LexerToken;

// Deinitialize and free a LexerToken
void LexerToken_free(LexerToken *tok);

typedef struct Lexer Lexer;

// Allocate and initialize a new config lexer.
// Returns NULL on error
Lexer *Lexer_new();
// Deinitialize and free a config lexer.
void Lexer_free(Lexer *l);

// Tokenize a chunk of MPL config.
// Returns 0 on success, nonzero on error
int Lexer_tokenize(Lexer *l, const char *chunk);

// Peek the next logical token
LexerToken *Lexer_peek(const Lexer *l);
// Consume the current logical token, advancing the lexer to the next token
void Lexer_consume(Lexer *l);
