#include "lexer.h"
#include "config/keybind/keycode.h"
#include "error.h"
#include "util/strtokn.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
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

// Append a lexer token to the end of lexer state
static void Lexer_append_(Lexer *l, LexerToken *tok) {
	LexerTokenNode *node = malloc(sizeof(LexerTokenNode));
	node->prev = l->head->prev;
	node->next = l->head;
	node->token = tok;

	l->head->prev->next = node;
}

// Tokenize a chunk of MPL config. This is where the lexing magic happens.
// Returns 0 on success, nonzero on error
int Lexer_tokenize(Lexer *l, const char *chunk) {
	setlocale(LC_ALL, "");

	static const char KEYWORD_BIND[] = "bind";
	static const char KEYWORD_TRUE[] = "true";
	static const char KEYWORD_FALSE[] = "false";


	for (const char *c = chunk; c != NULL; c++) {
		LexerToken *tok = malloc(sizeof(LexerToken));
#define TYPE(t) /* Set token type */ \
		tok->type = t; break
		
		switch (*c) {
			/* Symbols */
			case -1:
				TYPE(Tok_EOF);
			case ';':
				TYPE(Tok_Semi);
			case '\n':
				TYPE(Tok_LF);
			case '\'':
			case '"':
			{
				tok->type = Tok_StrLit;
				const char quote = *c;
				c++; // advance past quote
				char *end = strchr(c, quote);
				if (!end) {
					// Unterminated string
					return 1;
				}
				tok->str_lit = strndup(c, end - c);
				c = end;
				break;
			}
			case '=':
				TYPE(Tok_EQ);
			case '(':
				TYPE(Tok_Lparen);
			case ')':
				TYPE(Tok_Rparen);
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				tok->type = Tok_I32Lit;
				sscanf(c, "%d", &tok->i32_lit);
				const size_t tok_strlen = (*c == '-' ? 1 : 0) + floorl(log10(abs(tok->i32_lit)))+1;
				c += tok_strlen-1;
			}


			/* Keywords */
			case 'b':
				if (strncmp(c, KEYWORD_BIND, strlen(KEYWORD_BIND)) == 0) {
					// Parse keyname
					tok->type = Tok_Bind;
					c += strlen("bind");
					StrtoknState state;
					strtokn_init(&state, c + strlen("bind"), strlen(c));
					strtokn(&state, WHITESPACE);
					// Get keycode from keyname
					char *keyname = strndup(&state.s[state.offset], state.s_len);
					tok->keycode = parse_keycode(keyname);
					free(keyname);
					c += state.s_len-1;
					} else {
						return 1;
					}
				break;
			case 'f':
			{
				const size_t kw_len = strlen(KEYWORD_FALSE);
				if (strncmp(c, KEYWORD_FALSE, kw_len) == 0) {
					tok->type = Tok_BoolLit;
					tok->bool_lit = false;
					c += kw_len-1;
				} else {
					return 1;
				}
				break;
			}
			case 't':
			{
				const size_t kw_len = strlen(KEYWORD_TRUE);
				if (strncmp(c, KEYWORD_TRUE, kw_len) == 0) {
					tok->type = Tok_BoolLit;
					tok->bool_lit = true;
					c += kw_len-1;
				} else {
					return 1;
				}
				break;
			}
		}
	}

	return 0;
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
