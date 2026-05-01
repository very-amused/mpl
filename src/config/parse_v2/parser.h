#pragma once
#include "config/function/dictionary.h"
#include "config/setting/dictionary.h"
#include "error.h"
#include "config/parse_v2/lexer.h"
typedef struct Config Config;

// An error encountered while parsing mpl.conf
// line number included so we can build helpful error messages
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

#define PARSE_NODE_ID_ENUM(VARIANT) \
	VARIANT(ParseNodeID_ValueLit) \
	VARIANT(ParseNodeID_Root) \
	VARIANT(ParseNodeID_SettingStmt) \
	VARIANT(ParseNodeID_ShellStmt) \
	VARIANT(ParseNodeID_KeybindStmt) \
	VARIANT(ParseNodeID_FnCallList) \
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

typedef struct ParseNode ParseNode;
typedef struct ParseNode {
	enum ParseNodeID type;
	ParseNode *child;
	ParseNode *sibling;
#ifdef __cplusplus
	~ParseNode();
#endif
} ParseNode;

// Allocate and initialize a ParseNode with no child or sibling links
ParseNode *ParseNode_new(enum ParseNodeID type_id);
// Deinitialize and free a ParseNode and its children
//
// NOTE: This handles all node-specific data that needs to be freed,
// so calling ParseNode_rfree on a root node is sufficient to cleanup the entire parse tree.
void ParseNode_rfree(ParseNode *node);

// Create a deep copy of the parse tree starting at *node
ParseNode *ParseNode_rcopy(const ParseNode *node);

// Evaluate a function call
// fn_expr must be a ParseNode_FnCallExpr
enum Parser_ERR ParseNode_FnCallExpr_eval(ParseNode *fn_expr, void **ret);
// Get the function being called in a FnCallExpr
const ConfigFn *ParseNode_FnCallExpr_get_fn(ParseNode *fn_expr);

// A MPL config/shell parser
typedef struct Parser Parser;

// Create a new config parser that parses from lexer *l
Parser *Parser_new(Lexer *l, ConfigFnDict *fn_dict, ConfigSettingDict *setting_dict);
// Deinitialize and free a config parser
void Parser_free(Parser *p);


// Build a MPL config syntax tree from a stream of lexer tokens.
// Since multiple errors can occur, the *errors vector is provided
// so every error and its line number can be reported.
//
// Use [ParseNode_rfree] to free the parse tree.
ParseNode *Parser_parse(Parser *p, ParseLineError_Vec **errors);

// Flags that control what information is processed when walking a parse tree
typedef int ParserWalkFlags;
static const ParserWalkFlags
	Parser_WALK_SETTINGS = 1 << 0,
	Parser_WALK_KEYBINDS = 1 << 1,
	Parser_WALK_FUNCTIONS = 1 << 2,
	Parser_WALK_MACROS = 1 << 3,
	Parser_WALK_ALL = ~0;

// Walk and evaluate a parse tree
enum Parser_ERR Parser_walk(Parser *p, Config *config, ParserWalkFlags flags, ParseNode *tree);
