#include "parser.h"
#include "config/function/dictionary.h"
#include "config/parse_v2/lexer.h"
#include "config/parse_v2/types.h"
#include "config/setting/dictionary.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

/* #region ParserLineError */

static void ParseLineError_Vec_init(ParseLineError_Vec *v) {
	v->cap = 8;
	v->len = 0;
	v->data = malloc(v->cap * sizeof(ParseLineError));
}

void ParseLineError_Vec_deinit(ParseLineError_Vec *v) {
	free(v->data);
}

static void ParseLineError_Vec_push(ParseLineError_Vec *v, enum Parser_ERR err, size_t lineno) {
	if (v->len == v->cap)	{
		v->cap *= 2;
		v->data = realloc(v->data, v->cap * sizeof(ParseLineError));
	}
	ParseLineError *dst = &v->data[v->len];
	dst->type = err;
	dst->line = lineno;
	v->len++;
}

/* #endregion */

/* #region ParseNode types */

// A value literal with type information
typedef struct ParseNode_ValueLit ParseNode_ValueLit;

// Root parse node
// siblings: none
// children: SettingStmt | ShellStmt
typedef struct ParseNode ParseNode_Root;
// A setting definition
// siblings: other ParseNode_Root children (SettingStmt | ShellStmt)
// child: ValueLit (rhs)
typedef struct ParseNode_SettingStmt ParseNode_SettingStmt;
// A line of valid MPL shell. Can create a keybind or call any function/macro
// siblings: other ParseNode_Root children (SettingStmt | ShellStmt)
// child: KeybindStmt | FnCallExpr
typedef struct ParseNode ParseNode_ShellStmt;
// A keybind definition
// siblings: none
// child: FnCallList (rhs)
typedef struct ParseNode_KeybindStmt ParseNode_KeybindStmt;
// Calls to one or more function/macro(s) to be made in sequence
// siblings: none
// children: FnCallExpr
typedef struct ParseNode ParseNode_FnCallList;
// A call to one function/macro
// siblings: other FnCallExpr's if in a FnCallList
// child: ArgList
typedef struct ParseNode_FnCallExpr ParseNode_FnCallExpr; 
// A list of function arguments
// siblings: none
// children: ArgExpr
typedef struct ParseNode ParseNode_ArgList;
// A single function argument
// siblings: other ArgExpr's in the ArgList
// child: ValueLit | FnCallExpr
// TODO: we can use this to implement pipes
typedef struct ParseNode ParseNode_ArgExpr;

struct ParseNode_ValueLit {
	ParseNode *node;
	enum ConfigType type;
	union {
		int32_t val_i32;
		bool val_bool;
		char *val_str;
	};
};

struct ParseNode_SettingStmt {
	ParseNode node;
	const ConfigSetting *setting;
};

struct ParseNode_KeybindStmt {
	ParseNode node;
	wchar_t keycode; // (lhs)
};

struct ParseNode_FnCallExpr {
	ParseNode node;
	const ConfigFn *fn;
};

ParseNode *ParseNode_new(enum ParseNodeID type_id) {
	ParseNode *node;
	switch (type_id) {
	case ParseNodeID_ValueLit:
		node = malloc(sizeof(ParseNode_ValueLit));
		break;
	case ParseNodeID_Root:
		node = malloc(sizeof(ParseNode_Root));
		break;
	case ParseNodeID_SettingStmt:
		node = malloc(sizeof(ParseNode_SettingStmt));
		break;
	case ParseNodeID_ShellStmt:
		node = malloc(sizeof(ParseNode_ShellStmt));
		break;
	case ParseNodeID_KeybindStmt:
		node = malloc(sizeof(ParseNode_KeybindStmt));
		break;
	case ParseNodeID_FnCallList:
		node = malloc(sizeof(ParseNode_FnCallList));
		break;
	case ParseNodeID_FnCallExpr:
		node = malloc(sizeof(ParseNode_FnCallExpr));
		break;
	case ParseNodeID_ArgList:
		node = malloc(sizeof(ParseNode_ArgList));
		break;
	case ParseNodeID_ArgExpr:
		node = malloc(sizeof(ParseNode_ArgExpr));
		break;
	}

	memset(node, 0, sizeof(ParseNode));
	node->type = type_id;
	return node;
}

void ParseNode_rfree(ParseNode *node) {
	// NOTE: this is a DFS lol
	// Free all children
	for (ParseNode *child = node->child; child != NULL; child = child->sibling) {
		ParseNode_rfree(child);
	}
	// Free all siblings
	for (ParseNode *sib = node->sibling; sib != NULL; sib = sib->sibling) {
		ParseNode_rfree(sib);
	}

	// Free any extra data the node holds
	switch (node->type) {
	case ParseNodeID_ValueLit:
		{
			ParseNode_ValueLit *value_node = (ParseNode_ValueLit *)node;
			if (value_node->type == Config_STR) {
				free(value_node->val_str);
			}
			break;
		}
	default:
		break;
	}

	// Free the node itself
	free(node);
}

/* #endregion */

/* #region Parsing */

struct Parser {
	Lexer *lex;
	ConfigFnDict *fn_dict;
	ConfigSettingDict *setting_dict;
};


Parser *Parser_new(Lexer *l, ConfigFnDict *fn_dict, ConfigSettingDict *setting_dict) {
	Parser *p = malloc(sizeof(Parser));
	CHECK_ALLOC(p, NULL);
	p->lex = l;
	p->fn_dict = fn_dict;
	p->setting_dict = setting_dict;

	return p;
}
// Deinitialize and free a config parser
void Parser_free(Parser *p) {
	free(p);
}

static enum Parser_ERR Parser_parse_node(Parser *p, ParseNode *node);

ParseNode *Parser_parse(Parser *p, ParseLineError_Vec **errors) {
	ParseNode_Root *root = ParseNode_new(ParseNodeID_Root);

	// Initialize error vector
	*errors = malloc(sizeof(ParseLineError_Vec));
	ParseLineError_Vec_init(*errors);

	// Track line number by counting LF tokens
	size_t lineno = 1;

	ParseNode *tail = NULL; // last root child node, use tail->sibling to add new children

	for (const LexerToken *tok = Lexer_peek(p->lex); tok != NULL; tok = Lexer_peek(p->lex)) {
		ParseNode *child = NULL;

		switch (tok->type) {
			case Tok_Semi:
				break;
			case Tok_LF:
				lineno++;
				break;

			case Tok_Bind:
				child = ParseNode_new(ParseNodeID_ShellStmt);
				break;

			case Tok_Ident:
				if (ConfigFnDict_has(p->fn_dict, tok->ident)) {
					child = ParseNode_new(ParseNodeID_ShellStmt);
				} else if (ConfigSettingDict_has(p->setting_dict, tok->ident)) {
					child = ParseNode_new(ParseNodeID_SettingStmt);
				} else {
					ParseLineError_Vec_push(*errors, Parser_INVALID_IDENT, lineno);
				}
				break;

			default:
				ParseLineError_Vec_push(*errors, Parser_INVALID_TOKEN, lineno);
		}

		if (!child) {
			Lexer_consume(p->lex);
			continue;
		}

		// Parse the child node
		enum Parser_ERR err = Parser_parse_node(p, child);
		if (err != Parser_OK) {
			ParseNode_rfree(child);
			continue;
		}

		if (tail == NULL) {
			root->child = child;
		} else {
			tail->sibling = child;
		}
		tail = child;
	}

	return root;
}

static enum Parser_ERR Parser_parse_node(Parser *p, ParseNode *node) {
	LexerToken *tok = Lexer_peek(p->lex);

	switch (node->type) {
	case ParseNodeID_ValueLit:
		{
			ParseNode_ValueLit *lit = (ParseNode_ValueLit *)node;
			switch (tok->type) {
				case Tok_I32Lit:
					lit->type = Config_I32;
					lit->val_i32 = tok->i32_lit;
					break;
				case Tok_StrLit:
					lit->type = Config_STR;
					lit->val_str = tok->str_lit;
					tok->str_lit = NULL;
					break;
				case Tok_BoolLit:
					lit->type = Config_BOOL;
					lit->val_bool = tok->bool_lit;
					break;
				default:
					return Parser_INVALID_TOKEN;
			}
		}
	
	case ParseNodeID_SettingStmt:
		{
			// {ident}
			ParseNode_SettingStmt *stmt = (ParseNode_SettingStmt *)node;
			ConfigSettingDict_lookup(p->setting_dict, &stmt->setting, tok->ident);
			Lexer_consume(p->lex);
			if (!stmt->setting) {
				return Parser_INVALID_SETTING;
			}

			// =
			if (Lexer_peek(p->lex)->type != Tok_EQ){
				return Parser_INVALID_TOKEN;
			}
			Lexer_consume(p->lex);

			// {val}
			node->child = ParseNode_new(ParseNodeID_ValueLit);
			return Parser_parse_node(p, node->child);
		}

	case ParseNodeID_ShellStmt:
		{
			switch (tok->type) {
			case Tok_Bind:
				{
					node->child = ParseNode_new(ParseNodeID_KeybindStmt);
					Lexer_consume(p->lex);
					return Parser_parse_node(p, node->child);
				}
			case Tok_Ident:
				{
					if (ConfigFnDict_has(p->fn_dict, tok->ident)) {
						node->child = ParseNode_new(ParseNodeID_FnCallList);
						Lexer_consume(p->lex);
						return Parser_parse_node(p, node->child);
					} else {
						return Parser_INVALID_FUNCTION;
					}
				}
			default:
				return Parser_INVALID_TOKEN;
			}
		}

	case ParseNodeID_KeybindStmt:
		{
			ParseNode_KeybindStmt *stmt = (ParseNode_KeybindStmt *)node;
			// bind
			if (tok->type != Tok_Bind) {
				return Parser_INVALID_TOKEN;
			}
			Lexer_consume(p->lex);

			// {keysym}
			tok = Lexer_peek(p->lex);
			if (tok->type != Tok_Keysym) {
				return Parser_INVALID_TOKEN;
			}
			stmt->keycode = tok->keycode;
			Lexer_consume(p->lex);

			// =
			if (Lexer_peek(p->lex)->type != Tok_EQ) {
				return Parser_INVALID_TOKEN;
			}
			Lexer_consume(p->lex);

			// {FnCallList}
			node->child = ParseNode_new(ParseNodeID_FnCallList);
			return Parser_parse_node(p, node->child);
		}

	case ParseNodeID_FnCallList:
		{
			if (tok->type != Tok_Ident) {
				return Parser_INVALID_TOKEN;
			}

			ParseNode_FnCallExpr *tail = NULL;
			while (tok->type == Tok_Ident) {
				// Parse FnCallExpr
				ParseNode *fn_call = ParseNode_new(ParseNodeID_FnCallExpr);
				enum Parser_ERR err = Parser_parse_node(p, fn_call);
				if (err != Parser_OK) {
					return err;
				}

				// Append the fn call to the list
				if (tail) {
					tail->node.sibling = (ParseNode *)fn_call;
				} else {
					node->child = fn_call;
				}
				tail = (ParseNode_FnCallExpr *)fn_call;

				// Consume delim
				while (Lexer_peek(p->lex)->type == Tok_Semi) {
					Lexer_consume(p->lex);
				}
				tok = Lexer_peek(p->lex);
			}
		}
	
	case ParseNodeID_FnCallExpr:
		{
			ParseNode_FnCallExpr *expr = (ParseNode_FnCallExpr *)node;
			// {ident}
			if (tok->type != Tok_Ident) {
				return Parser_INVALID_TOKEN;
			}
			ConfigFnDict_lookup(p->fn_dict, &expr->fn, tok->ident);
			Lexer_consume(p->lex);
			if (!expr->fn) {
				return Parser_INVALID_FUNCTION;
			}

			// (
			tok = Lexer_peek(p->lex);
			if (tok->type != Tok_Lparen) {
				return Parser_INVALID_TOKEN;
			}
			Lexer_consume(p->lex);

			// ) or ArgsList
			tok = Lexer_peek(p->lex);
			if (tok->type == Tok_Rparen) {
				// FIXME: we need to register the numer of args a function takes in the dictionary
			}
		}
	}

	return -1;
}

/* #endregion */
