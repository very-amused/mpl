#include "parser.h"
#include "config/config.h"
#include "config/function/function.h"
#include "config/function/dictionary.h"
#include "config/parse_v2/lexer.h"
#include "config/parse_v2/types.h"
#include "config/setting/dictionary.h"
#include "error.h"
#include "util/log.h"
#include <assert.h>
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
typedef struct ParseNode_ArgList ParseNode_ArgList;
// A single function argument
// siblings: other ArgExpr's in the ArgList
// child: ValueLit | FnCallExpr
// TODO: we can use this to implement pipes
typedef struct ParseNode ParseNode_ArgExpr;

struct ParseNode_ValueLit {
	ParseNode node;
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

struct ParseNode_ArgList {
	ParseNode node;
	const ConfigFn *fn; // used to check argument count and types
};

// Get the true (full) size of a ParseNode using its type ID
static const size_t ParseNode_size(enum ParseNodeID type_id) {
	switch (type_id) {
	case ParseNodeID_ValueLit:
		return sizeof(ParseNode_ValueLit);
	case ParseNodeID_Root:
		return sizeof(ParseNode_Root);
	case ParseNodeID_SettingStmt:
		return sizeof(ParseNode_SettingStmt);
	case ParseNodeID_ShellStmt:
		return sizeof(ParseNode_ShellStmt);
	case ParseNodeID_KeybindStmt:
		return sizeof(ParseNode_KeybindStmt);
	case ParseNodeID_FnCallList:
		return sizeof(ParseNode_FnCallList);
	case ParseNodeID_FnCallExpr:
		return sizeof(ParseNode_FnCallExpr);
	case ParseNodeID_ArgList:
		return sizeof(ParseNode_ArgList);
	case ParseNodeID_ArgExpr:
		return sizeof(ParseNode_ArgExpr);
	}
}

ParseNode *ParseNode_new(enum ParseNodeID type_id) {
	const size_t true_size = ParseNode_size(type_id);
	ParseNode *node = malloc(true_size);
	memset(node, 0, true_size);
	node->type = type_id;
	return node;
}

void ParseNode_rfree(ParseNode *node) {
	static int depth = 0;
	LOG(Verbosity_DEBUG, "ParseNode_rfree called on a %s (depth = %d)\n", ParseNodeID_name(node->type), depth);
	// NOTE: this is a DFS lol
	// Free children
	if (node->child != NULL) {
		depth++;
		ParseNode_rfree(node->child);
		depth--;
	}
	// Free siblings
	if (node->sibling != NULL) {
		ParseNode_rfree(node->sibling);
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

ParseNode *ParseNode_rcopy(ParseNode *node) {
	// Copy the node itself
	ParseNode *dst = ParseNode_new(node->type);
	memcpy(node, dst, ParseNode_size(node->type));

	// NOTE: this is still a DFS lol
	// Copy children
	if (node->child) {
		dst->child = ParseNode_rcopy(node->child);
	}
	// Copy siblings
	if (node->sibling) {
		dst->sibling = ParseNode_rcopy(node->sibling);
	}

	return node;
}

void *ParseNode_eval(ParseNode_Callable *node) {
	// Handle FnCallList's as a sequence
	if (node->type == ParseNodeID_FnCallList) {
		for (ParseNode *child = node->child; child != NULL; child = child->child) {
			ParseNode_eval(child);
		}
		return NULL;
	}

	// Otherwise, make sure we're working with a FnCallExpr
	if (node->type != ParseNodeID_FnCallExpr) {
		LOG(Verbosity_NORMAL, "Error: ParseNode_eval called with invalid node type %s\n", ParseNodeID_name(node->type));
		return NULL;
	}

	// Form arguments for the function
	// FIXME: this is where we have to overhaul the way ConfigFn's accept args.

	// TODO
	return NULL;
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

		if (tail) {
			tail->sibling = child;
		} else {
			root->child = child;
		}
		tail = child;
	}

	return root;
}

static enum Parser_ERR Parser_parse_node(Parser *p, ParseNode *node) {
	LexerToken *tok = Lexer_peek(p->lex);

	LOG(Verbosity_DEBUG, "Parsing node: %s\n", ParseNodeID_name(node->type));

	switch (node->type) {
	case ParseNodeID_Root:
		{
			// Use parser_parse instead
			return Parser_INVALID_NODE;
		}
		break;

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
			Lexer_consume(p->lex);
		}
		break;
	
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
		break;

	case ParseNodeID_ShellStmt:
		{
			switch (tok->type) {
			case Tok_Bind:
				{
					node->child = ParseNode_new(ParseNodeID_KeybindStmt);
					return Parser_parse_node(p, node->child);
				}
			case Tok_Ident:
				{
					if (ConfigFnDict_has(p->fn_dict, tok->ident)) {
						node->child = ParseNode_new(ParseNodeID_FnCallList);
						return Parser_parse_node(p, node->child);
					} else {
						return Parser_INVALID_FUNCTION;
					}
				}
			default:
				return Parser_INVALID_TOKEN;
			}
		}
		break;

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
		break;

	case ParseNodeID_FnCallList:
		{
			if (tok->type != Tok_Ident) {
				LOG(Verbosity_DEBUG, "in FnCallList: tok->type = %s\n", LexerToken_t_name(tok->type));
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
					tail->node.sibling = fn_call;
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
		break;
	
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

			// ArgList
			if (expr->fn->argc > 0) {
				expr->node.child = ParseNode_new(ParseNodeID_ArgList);
				ParseNode_ArgList *arg_list = (ParseNode_ArgList *)expr->node.child;
				arg_list->fn = expr->fn;
				enum Parser_ERR err = Parser_parse_node(p, expr->node.child);
				if (err != Parser_OK) {
					return err;
				}
			}

			// )
			tok = Lexer_peek(p->lex);
			if (tok->type != Tok_Rparen) {
				return Parser_INVALID_TOKEN;
			}
			Lexer_consume(p->lex);
		}
		break;

	case ParseNodeID_ArgList:
		{
			ParseNode_ArgList *arg_list = (ParseNode_ArgList *)node;

			size_t n_parsed = 0; // # of arguments parsed
			ParseNode_ArgExpr *tail = NULL;
			while (Lexer_peek(p->lex)->type != Tok_Rparen) {
				// Parse ArgExpr
				ParseNode *arg = ParseNode_new(ParseNodeID_ArgExpr);
				enum Parser_ERR err = Parser_parse_node(p, arg);
				if (err != Parser_OK) {
					return err;
				}

				// Append the arg to the list
				if (tail) {
					tail->sibling = arg;
				} else {
					node->child = arg;
				}
				tail = (ParseNode_ArgExpr *)arg;

				// Consume delim
				if (++n_parsed < arg_list->fn->argc) {
					tok = Lexer_peek(p->lex);
					if (tok->type != Tok_Comma) {
						return Parser_INVALID_TOKEN;
					}
					Lexer_consume(p->lex);
				}
			}
		}
		break;

	case ParseNodeID_ArgExpr:
		{
			// FIXME: we have no type checking of function args right now!
			switch (tok->type) {
			case Tok_I32Lit:
			case Tok_BoolLit:
			case Tok_StrLit:
				{
					node->child = ParseNode_new(ParseNodeID_ValueLit);
					return Parser_parse_node(p, node->child);
				}
				break;

			case Tok_Ident:
				{
					node->child = ParseNode_new(ParseNodeID_FnCallExpr);
					return Parser_parse_node(p, node->child);
				}
				break;

			default:
				return Parser_INVALID_TOKEN;
			}
		}
	}

	return Parser_OK;
}

/* #endregion */

/* #region Parse tree traversal */


enum Parser_ERR Parser_walk(Parser *p, Config *config, ParserWalkFlags flags, ParseNode *tree) {
	switch (tree->type)	{
	case ParseNodeID_Root:
		{
			for (ParseNode *child = tree->child; child != NULL; child = child->sibling) {
				enum Parser_ERR err = Parser_walk(p, config, flags, child);
				if (err != Parser_OK) {
					return err;
				}
			}
		}
		break;

	case ParseNodeID_SettingStmt:
		{
			if (!(flags & Parser_WALK_SETTINGS)) {
				return Parser_OK;
			}

			ParseNode_SettingStmt *stmt = (ParseNode_SettingStmt *)tree;
			ParseNode_ValueLit *val = (ParseNode_ValueLit *)stmt->node.child;
			// Validate we have a value for the setting and its type is correct
			if (!val || val->node.type != ParseNodeID_ValueLit) {
				return Parser_SYNTAX_ERR;
			}
			if (val->type != stmt->setting->type) {
				// type mismatch
				return Parser_TYPE_ERR;
			}

			void *dst = (unsigned char *)(&config->settings) + stmt->setting->struct_offset;
			switch (val->type) {
			case Config_I32:
				*(int32_t *)dst = val->val_i32;
				break;

			case Config_STR:
				*(char **)dst = strdup(val->val_str);
				break;

			case Config_BOOL:
				*(bool *)dst = val->val_bool;
				break;
			}
		}
		break;

	case ParseNodeID_ShellStmt:
		{
			return Parser_walk(p, config, flags, tree->child);
		}
		break;

	case ParseNodeID_KeybindStmt:
		{
			if (!(flags & Parser_WALK_KEYBINDS)) {
				return Parser_OK;
			}

			ParseNode_KeybindStmt *stmt = (ParseNode_KeybindStmt *)tree;
			// FIXME: we need a ParseNode_rcopy
		}
		break;
	}


	//TODO
	return Parser_OK;
}
