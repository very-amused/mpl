#include "parser.h"
#include "config/parse_v2/lexer.h"
#include "error.h"
#include <stdlib.h>

struct ParseNode_Root_Child {
	enum ParseNodeID type; // SettingStmt | ShellStmt
	union {
		ParseNode_SettingStmt *setting;
		ParseNode_ShellStmt *shell;
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
int Parser_parse_Config(Parser *p, ParseNode_Root *node) {
}

int Parser_parse_SettingStmt(Parser *p, ParseNode_SettingStmt *node);
int Parser_parse_ShellStmt(Parser *p, ParseNode_ShellStmt *node);

int Parser_parse_KeybindStmt(Parser *p, ParseNode_KeybindStmt *node);
int Parser_parse_FnCallExpr(Parser *p, ParseNode_FnCallExpr *node);

int Parser_parse_ArgList(Parser *p, ParseNode_ArgList *node);
int Parser_parse_ArgExpr(Parser *p, ParseNode_ArgExpr *node);
