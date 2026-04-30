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
				child = ParseNode_new(ParseNodeID_KeybindStmt);
				break;

			case Tok_Ident:
				if (ConfigFnDict_has(p->fn_dict, tok->ident)) {
					child = ParseNode_new(ParseNodeID_ShellStmt);
				} else if (ConfigSettingDict_has(p->setting_dict, tok->ident)) {
					child = ParseNode_new(ParseNodeID_ShellStmt);
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
	// TODO

	return -1;
}

#if 0

// Config => SettingStmt \n Config
// Config => ShellStmt \n Config
// Config => \n
// Config => ;
// Config => EOF
void Parser_parse_Config(Parser *p, ParseNode_Root *node,
		ConfigFnDict *fn_dict, ConfigSettingDict *setting_dict,
		ParseLineError_Vec **errors) {

	// Initialize error vector
	*errors = malloc(sizeof(ParseLineError_Vec));
	ParseLineError_Vec_init(*errors);

	// Track line number by counting LF tokens
	size_t lineno = 1;

	struct ParseNode_Root_Child *last_child = NULL; // used to append to SLL node->children
	for (const LexerToken *tok = Lexer_peek(p->lex); tok != NULL; tok = Lexer_peek(p->lex)) {
		// Figure out the type of our next child to parse
		struct ParseNode_Root_Child child;
		memset(&child, 0, sizeof(struct ParseNode_Root_Child));

		switch (tok->type) {
			case Tok_Bind:
				child.type = ParseNodeID_ShellStmt;
				break;
			case Tok_Ident:
				if (ConfigFnDict_has(fn_dict, tok->ident)) {
					// If this ident belongs to a function, our child is a ShellStmt.
					child.type = ParseNodeID_ShellStmt;
				} else if (ConfigSettingDict_has(setting_dict, tok->ident)) {
					child.type = ParseNodeID_SettingStmt;
				} else {
					ParseLineError_Vec_push(*errors, Parser_INVALID_IDENT, lineno);
					Lexer_consume(p->lex);
					continue;
				}
				break;
			case Tok_LF:
				lineno++;
			case Tok_Semi:
				Lexer_consume(p->lex);
				continue;
			default:
				Lexer_consume(p->lex);
				ParseLineError_Vec_push(*errors, Parser_INVALID_TOKEN, lineno);
				continue;
		}

		// Parse functions are responsible for advancing the lexer regardless of error or success
		enum Parser_ERR result;
		switch (child.type) {
			case ParseNodeID_SettingStmt:
				child.setting_stmt = malloc(sizeof(ParseNode_SettingStmt));
				result = Parser_parse_SettingStmt(p, child.setting_stmt, setting_dict);
				break;
			case ParseNodeID_ShellStmt:
				child.shell_stmt = malloc(sizeof(ParseNode_ShellStmt));
				result = Parser_parse_ShellStmt(p, child.shell_stmt);
				break;
			default:
				result = Parser_INVALID_NODE;
		}
		if (result != Parser_OK) {
			// If we fail to parse a setting or shell statement, advance to the next line
			while (Lexer_peek(p->lex)->type != Tok_LF) {
				Lexer_consume(p->lex);
			}
			ParseLineError_Vec_push(*errors, result, lineno);
			continue;
		}

		// I love this variable name
		struct ParseNode_Root_Child *heap_child = malloc(sizeof(struct ParseNode_Root_Child));
		*heap_child = child;
		if (last_child) {
			last_child->next = heap_child;
		} else {
			node->children = heap_child;
		}
		last_child = heap_child;
	}
}

enum Parser_ERR Parser_parse_SettingStmt(Parser *p, ParseNode_SettingStmt *node,
		ConfigSettingDict *dict) {
	// Lookup setting by ident
	const LexerToken *tok = Lexer_peek(p->lex);
	if (tok->type != Tok_Ident) {
		return Parser_INVALID_TOKEN;
	}
	ConfigSettingDict_lookup(dict, &node->setting, tok->ident);
	if (node->setting == NULL) {
		return Parser_INVALID_SETTING;
	}
	Lexer_consume(p->lex);

	// =
	if (Lexer_peek(p->lex)->type != Tok_EQ) {
		return Parser_SYNTAX_ERR;
	}
	Lexer_consume(p->lex);

	// Validate value type and store value
	tok = Lexer_peek(p->lex);
	switch (node->setting->type) {
	case Config_STR:
		if (tok->type != Tok_StrLit) {
			return Parser_TYPE_ERR; 
		}
		node->value_literal = strdup(tok->str_lit);
		break;
	case Config_I32:
		if (tok->type != Tok_I32Lit) {
			return Parser_TYPE_ERR;
		}
		node->value_literal = malloc(sizeof(int32_t));
		*(int32_t *)node->value_literal = tok->i32_lit;
		break;
	case Config_BOOL:
		if (tok->type != Tok_BoolLit) {
			return Parser_TYPE_ERR;
		}
		node->value_literal = malloc(sizeof(bool));
		*(bool *)node->value_literal = tok->bool_lit;
	}

	return Parser_OK;
}

#endif

/* #endregion */
