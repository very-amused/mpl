#include "parser.h"
#include "config/function/dictionary.h"
#include "config/parse_v2/lexer.h"
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
	char *setting_ident;
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

// Config => SettingStmt \n Config
// Config => ShellStmt \n Config
// Config => \n
// Config => ;
// Config => EOF
enum Parser_ERR Parser_parse_Config(Parser *p, ParseNode_Root *node, ConfigFnDict *fn_dict) {
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
				// If this ident belongs to a function, our child is a ShellStmt.
				// Otherwise, our child must be a SettingStmt.
				child.type = ConfigFnDict_has(fn_dict, tok->ident) ? ParseNodeID_ShellStmt : ParseNodeID_SettingStmt;
				break;
			case Tok_LF:
			case Tok_Semi:
				continue;
			default:
				return Parser_INVALID_TOKEN;
		}

		enum Parser_ERR result;
		switch (child.type) {
			case ParseNodeID_SettingStmt:
				child.setting_stmt = malloc(sizeof(ParseNode_SettingStmt));
				result = Parser_parse_SettingStmt(p, child.setting_stmt);
				break;
			case ParseNodeID_ShellStmt:
				child.shell_stmt = malloc(sizeof(ParseNode_ShellStmt));
				result = Parser_parse_ShellStmt(p, child.shell_stmt);
				break;
			default:
				return Parser_INVALID_NODE; // should never happen
		}
		if (result != Parser_OK) {
			return result;
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

	return Parser_OK;
}

enum Parser_ERR Parser_parse_SettingStmt(Parser *p, ParseNode_SettingStmt *node) {
	const LexerToken *tok = Lexer_peek(p->lex);
	if (tok->type != Tok_Ident) {
		return Parser_INVALID_TOKEN;
	}

	// Lookup setting by ident
	// TODO: we need a SettingsDict
	return -1;
}
enum Parser_ERR Parser_parse_ShellStmt(Parser *p, ParseNode_ShellStmt *node) {
	// TODO
	return -1;
}

enum Parser_ERR Parser_parse_KeybindStmt(Parser *p, ParseNode_KeybindStmt *node);
enum Parser_ERR Parser_parse_FnCallExpr(Parser *p, ParseNode_FnCallExpr *node);

enum Parser_ERR Parser_parse_ArgList(Parser *p, ParseNode_ArgList *node);
enum Parser_ERR Parser_parse_ArgExpr(Parser *p, ParseNode_ArgExpr *node);
