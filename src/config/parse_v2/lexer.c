#include "lexer.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

void LexerToken_free(LexerToken *tok) {
	switch (tok->type) {
	case Tok_StrLit:
		free(tok->str_lit);
		break;
	case Tok_SettingIdent:
	case Tok_FnIdent:
		free(tok->ident);
		break;
	default:
		break;
	}

	free(tok);
}

// A DLL node for a lexer token
typedef struct LexerTokenNode LexerTokenNode;
struct LexerTokenNode {
	LexerToken *token;
	LexerTokenNode *prev;
	LexerTokenNode *next;
};

struct Lexer {
	LexerTokenNode *head; // Sentinel node that holds no tokern		
	LexerTokenNode *cur; // Current logical token used for Lexer_peek
};

Lexer *Lexer_new() {
	Lexer *lex = malloc(sizeof(Lexer));
	CHECK_ALLOC(lex, NULL);
	memset(lex, 0, sizeof(Lexer));

	// initialize DLL
	lex->head = malloc(sizeof(LexerTokenNode));
	lex->head->token = NULL;
	lex->head->prev = lex->head->next = lex->head;
	lex->cur = lex->head;

	return lex;
}
// Deinitialize and free a config lexer.
void Lexer_free(Lexer *l) {
	while (l->cur != l->head) {
		LexerTokenNode *next = l->cur->next;
		LexerToken_free(l->cur->token);
		free(l->cur);
		l->cur = next;
	}
}

// Tokenize a chunk of MPL config. This is where the lexing magic happens.
// Returns 0 on success, nonzero on error
int Lexer_tokenize(Lexer *l, const char *chunk) {
	// TODO
}

// Peek the next logical token
const LexerToken *Lexer_peek(const Lexer *l) {
	return l->cur->token;
}
// Consume the current logical token, causing the lexer to advance to the next token
void Lexer_advance(Lexer *l) {
	if (l->cur == l->head) {
		return;
	}

	LexerTokenNode *old = l->cur;
	old->prev->next = old->next;
	old->next->prev = old->prev;
	l->cur = old->next;
	LexerToken_free(old->token);
	free(old);
}
