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

// Root parse node
typedef struct ParseNode_Root ParseNode_Root;
// A setting definition
typedef struct ParseNode_SettingStmt ParseNode_SettingStmt;
// A line of valid MPL shell. Can create a keybind or call any function/macro
typedef struct ParseNode_ShellStmt ParseNode_ShellStmt;
// A keybind definition
typedef struct ParseNode_KeybindStmt ParseNode_KeybindStmt;
// A call to one or more functions/macros
typedef struct ParseNode_FnCallExpr ParseNode_FnCallExpr;
// A list of function arguments
typedef struct ParseNode_ArgList ParseNode_ArgList;
// A single function argument
typedef struct ParseNode_ArgExpr ParseNode_ArgExpr;

// A MPL config/shell parser
typedef struct Parser Parser;

// Create a new config parser that parses from lexer *l
Parser *Parser_new(Lexer *l);
// Deinitialize and free a config parser
void Parser_free(Parser *p);

// these function names aren't gonna make much sense without reading MPL's grammar spec

int Parser_parse_Config(Parser *p, ParseNode_Root *node);

int Parser_parse_SettingStmt(Parser *p, ParseNode_SettingStmt *node);
int Parser_parse_ShellStmt(Parser *p, ParseNode_ShellStmt *node);

int Parser_parse_KeybindStmt(Parser *p, ParseNode_KeybindStmt *node);
int Parser_parse_FnCallExpr(Parser *p, ParseNode_FnCallExpr *node);

int Parser_parse_ArgList(Parser *p, ParseNode_ArgList *node);
int Parser_parse_ArgExpr(Parser *p, ParseNode_ArgExpr *node);
