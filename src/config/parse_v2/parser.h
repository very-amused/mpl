#pragma once

#include "config/parse_v2/lexer.h"
typedef struct Parser Parser;

// Create a new config parser that parses from lexer *l
Parser *Parser_new(Lexer *l);
// Deinitialize and free a config parser
void Parser_free(Parser *p);

// these function names aren't gonna make much sense without reading MPL's grammar spec

int Parser_parse_Config(Parser *p);

int Parser_parse_SettingStmt(Parser *p);
int Parser_parse_ShellStmt(Parser *p);

int Parser_parse_KeybindStmt(Parser *p);
int Parser_parse_FnCallExpr(Parser *p);

int Parser_parse_ArgList(Parser *p);
int Parser_parse_ArgExpr(Parser *p);
