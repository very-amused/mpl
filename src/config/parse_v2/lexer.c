#include "lexer.h"
#include "error.h"
#include "keycode.h"
#include "util/log.h"

#include <ctype.h>
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
	case Tok_Ident:
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
	LexerTokenNode *head; // Sentinel node that holds no token. head->next is the token used for peek		
};

Lexer *Lexer_new() {
	Lexer *lex = malloc(sizeof(Lexer));
	CHECK_ALLOC(lex, NULL);
	memset(lex, 0, sizeof(Lexer));

	// initialize DLL
	lex->head = malloc(sizeof(LexerTokenNode));
	lex->head->token = NULL;
	lex->head->prev = lex->head->next = lex->head;

	return lex;
}
// Deinitialize and free a config lexer.
void Lexer_free(Lexer *l) {
	// Free all tokens
	LexerTokenNode *tok = l->head->next;
	while (tok != l->head) {
		LexerTokenNode *next = tok->next;
		LexerToken_free(tok->token);
		free(tok);
		tok = next;
	}
	free(l->head);

	// Free the lexer itself
	free(l);
}

// Append a lexer token to the end of lexer state
static void Lexer_append(Lexer *l, LexerToken *tok) {
	LexerTokenNode *node = malloc(sizeof(LexerTokenNode));
	node->prev = l->head->prev;
	node->next = l->head;
	node->token = tok;

	l->head->prev->next = node;
	l->head->prev = node;
}

// Tokenize a chunk of MPL config. This is where the lexing magic happens.
// Returns 0 on success, nonzero on error
enum Parser_ERR Lexer_tokenize(Lexer *l, const char *chunk) {
	setlocale(LC_ALL, "");

	static const char KEYWORD_BIND[] = "bind";
	static const char KEYWORD_TRUE[] = "true";
	static const char KEYWORD_FALSE[] = "false";

	const char *c = chunk;
	while (*c != '\0') {	
		// Eat whitespace and comments
		switch (*c) {
			case ' ':
			case '\t':
				c++;
				continue; // advance past non-breaking whitespace
			case '#':
			{
				do {
					c++;
				} while (!(*c == '\0' || *(c) == '\n'));
				continue;
			}
		}

		LexerToken *tok = malloc(sizeof(LexerToken));
		tok->type = -1;
		switch (*c) {
			/* Symbols */
			case '\r':
			case '\n':
				tok->type = Tok_LF;
				c++;
				break;
			case ';':
				tok->type = Tok_Semi;
				c++;
				break;
			case '\'':
			case '"':
			{
				// string literal
				tok->type = Tok_StrLit;
				const char quote = *c;
				c++; // advance past opening quote
				const char *end = strchr(c, quote);
				if (!end) {
					return Parser_UNTERMINATED_STR;
				}
				tok->str_lit = strndup(c, end - c);
				c = end+1; // advance past closing quote
				break;
			}
			case '=':
				tok->type = Tok_EQ;
				c++;
				break;
			case '(':
				tok->type = Tok_Lparen;
				c++;
				break;
			case ')':
				tok->type = Tok_Rparen;
				c++;
				break;
			case ',':
				tok->type = Tok_Comma;
				c++;
				break;
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
				c += tok_strlen;
			}


			/* Keywords */
			case 'b':
			{
				const size_t kw_len = sizeof(KEYWORD_BIND)-1;
				if (strncmp(c, KEYWORD_BIND, kw_len) == 0) {
					// Append bind token
					tok->type = Tok_Bind;
					Lexer_append(l, tok);
					c += kw_len;

					// Parse keyname token
					tok = malloc(sizeof(LexerToken));
					tok->type = Tok_Keysym;
					while (*c == ' ' || *c == '\t') {
						c++;
					}
					const char *end = c;
					while (!(*end == ' ' || *end == '\t')) {
						end++;
					}
					// Get keycode from keyname
					char *keyname = strndup(c, end - c);
					tok->keycode = parse_keycode(keyname);
					free(keyname);
					c = end;
				}
				break;
			}
			case 'f':
			{
				const size_t kw_len = sizeof(KEYWORD_FALSE)-1;
				if (strncmp(c, KEYWORD_FALSE, kw_len) == 0) {
					tok->type = Tok_BoolLit;
					tok->bool_lit = false;
					c += kw_len;
				}
				break;
			}
			case 't':
			{
				const size_t kw_len = sizeof(KEYWORD_TRUE)-1;
				if (strncmp(c, KEYWORD_TRUE, kw_len) == 0) {
					tok->type = Tok_BoolLit;
					tok->bool_lit = true;
					c += kw_len;
				}
				break;
			}
		}

		if (tok->type == -1) {
			// If nothing else has matched, try to read ident
			const char *end = c;
			do {
				end++;
			} while (isalpha(*end) || (*end >= '0' && *end <= '9') || *end == '_');
			tok->type = Tok_Ident;
			tok->ident = strndup(c, end - c);
			c = end;
		}

#ifdef MPL_PARSING_DEBUG
		LOG(Verbosity_DEBUG, "tok->type = %s\n", LexerToken_t_name(tok->type));
		if (cli_args.verbosity >= Verbosity_DEBUG) {
			switch (tok->type) {
			case Tok_Ident:
				LOG(Verbosity_DEBUG, "tok->ident = %s\n", tok->ident);
				break;
			case Tok_I32Lit:
				LOG(Verbosity_DEBUG, "tok->i32_lit = %d\n", tok->i32_lit);
				break;
			case Tok_StrLit:
				LOG(Verbosity_DEBUG, "tok->str_lit = %s\n", tok->str_lit);
				break;
			case Tok_BoolLit:
				LOG(Verbosity_DEBUG, "tok->bool_lit = %s\n", tok->bool_lit ? "true" : "false");
				break;
			default:
				break;
			}
		}
#endif
		// Append the token to our list
		Lexer_append(l, tok);
	}

	return Parser_OK;
}

LexerToken *Lexer_peek(const Lexer *l) {
	LexerTokenNode *cur = l->head->next;
	return cur->token;
}
void Lexer_consume(Lexer *l) {
	LexerTokenNode *cur = l->head->next;
	if (cur == l->head) {
		return;
	}

	// Cut node out of list
	l->head->next = cur->next;
	cur->next->prev = l->head;
	// Free the node
	LexerToken_free(cur->token);
	free(cur);
}

size_t Lexer_count(Lexer *l, enum LexerToken_t type_id) {
	// Count the number of tokens that match type_id
	size_t count = 0;
	for (LexerTokenNode *node = l->head->next; node != l->head; node = node->next) {
		if (node->token->type == type_id) {
			count++;
		}
	}

	return count;
}

