#include "parser.h"
#include "config/parse_v2/lexer.h"
#include "error.h"
#include <stdlib.h>

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
int Parser_parse_Config(Parser *p) {
	// TODO

	return 1;
}

int Parser_parse_SettingStmt(Parser *p);
int Parser_parse_ShellStmt(Parser *p);

int Parser_parse_KeybindStmt(Parser *p);
int Parser_parse_FnCallExpr(Parser *p);

int Parser_parse_ArgList(Parser *p);
int Parser_parse_ArgExpr(Parser *p);
