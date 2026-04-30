#include "parser.h"
#include "config/function/dictionary.h"
#include "config/parse_v2/lexer.h"
#include "config/parse_v2/types.h"
#include "config/setting/dictionary.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

struct ParseNode_Root_Child {
	struct ParseNode_Root_Child *next; // Set to NULL for the last child
	enum ParseNodeID type; // SettingStmt | ShellStmt
	union {
		ParseNode_SettingStmt *setting_stmt;
		ParseNode_ShellStmt *shell_stmt;
	};
};

struct ParseNode_Root {
	// SettingStmt | ShellStmt
	struct ParseNode_Root_Child *children;
};

struct ParseNode_SettingStmt {
	const ConfigSetting *setting;
	void *value_literal;
};

struct ParseNode_ShellStmt {
	enum ParseNodeID type; // KeybindStmt | FnCallExpr
	union {
		ParseNode_KeybindStmt *keybind;
		ParseNode_FnCallExpr *fn_call;
	};
};

struct ParseNode_KeybindStmt {
	wchar_t keycode;
	ParseNode_FnCallExpr *fn_call;
};

struct ParseNode_FnCallExpr {
	bool is_sequence; // This is a sequence of function calls. Check *next for the next call
	ParseNode_FnCallExpr *next; // used when [is_sequence == true]. NULL at the end of the sequence

	char *fn_ident;
	ParseNode_ArgList *args;
};

struct ParseNode_ArgList {
	ParseNode_ArgList *next; // set to NULL for the last argument
	ParseNode_ArgExpr *arg;
};

struct ParseNode_ArgExpr {
	bool is_fn_call; // this argument the result of a/an function call(s)
	union {
		ParseNode_FnCallExpr *fn_call;
		void *value_literal;
	};
};

struct Parser {
	Lexer *lex;
};


Parser *Parser_new(Lexer *l) {
	Parser *p = malloc(sizeof(Parser));
	CHECK_ALLOC(p, NULL);
	p->lex = l;

	return p;
}
// Deinitialize and free a config parser
void Parser_free(Parser *p) {
	free(p);
}

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
enum Parser_ERR Parser_parse_ShellStmt(Parser *p, ParseNode_ShellStmt *node) {
	// TODO
	return -1;
}

enum Parser_ERR Parser_parse_KeybindStmt(Parser *p, ParseNode_KeybindStmt *node);
enum Parser_ERR Parser_parse_FnCallExpr(Parser *p, ParseNode_FnCallExpr *node);

enum Parser_ERR Parser_parse_ArgList(Parser *p, ParseNode_ArgList *node);
enum Parser_ERR Parser_parse_ArgExpr(Parser *p, ParseNode_ArgExpr *node);
