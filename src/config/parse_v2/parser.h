#pragma once
#include "error.h"
#include "config/parse_v2/lexer.h"

#define PARSE_NODE_ID_ENUM(VARIANT) \
	VARIANT(ParseNodeID_Root) \
	VARIANT(ParseNodeID_SettingStmt) \
	VARIANT(ParseNodeID_ShellStmt) \
	VARIANT(ParseNodeID_KeybindStmt) \
	VARIANT(ParseNodeID_FnCallExpr) \
	VARIANT(ParseNodeID_ArgList) \
	VARIANT(ParseNodeID_ArgExpr)

enum ParseNodeID {
	PARSE_NODE_ID_ENUM(ENUM_VAL)
};

static inline const char *ParseNodeID_name(enum ParseNodeID id) {
	switch (id) {
		PARSE_NODE_ID_ENUM(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

#undef PARSE_NODE_ID_ENUM


// A MPL config/shell parser
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
