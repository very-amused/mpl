#pragma once
#include "config/function/dictionary.h"
#include "config/setting/dictionary.h"
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

typedef struct ParseLineError {
	enum Parser_ERR type;
	size_t line;
} ParseLineError;

// A vector of parse errors.
// Using this prevents any single error from halting the main parse routine
typedef struct ParseLineError_Vec {
	size_t cap;
	size_t len;
	ParseLineError *data;
} ParseLineError_Vec;

// Deinitialize a ParseLineError_Vec for freeing
void ParseLineError_Vec_deinit(ParseLineError_Vec *vec);

// Parse a MPL config syntax tree.
// Since multiple errors can occur, the *errors vector is provided
// so every error and its line number can be reported.
//
// Use [ParseLineError_Vec_deinit] to deinitialize the error vector for freeing
void Parser_parse_Config(Parser *p, ParseNode_Root *node,
		ConfigFnDict *fn_dict, ConfigSettingDict *setting_dict,
		ParseLineError_Vec **errors);

enum Parser_ERR Parser_parse_SettingStmt(Parser *p, ParseNode_SettingStmt *node,
		ConfigSettingDict *dict);
enum Parser_ERR Parser_parse_ShellStmt(Parser *p, ParseNode_ShellStmt *node);

enum Parser_ERR Parser_parse_KeybindStmt(Parser *p, ParseNode_KeybindStmt *node);
enum Parser_ERR Parser_parse_FnCallExpr(Parser *p, ParseNode_FnCallExpr *node);

enum Parser_ERR Parser_parse_ArgList(Parser *p, ParseNode_ArgList *node);
enum Parser_ERR Parser_parse_ArgExpr(Parser *p, ParseNode_ArgExpr *node);
