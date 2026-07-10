#include "parser.h"
#include "config/config.h"
#include "config/function/function.h"
#include "config/function/dictionary.h"
#include "config/memory.h"
#include "config/parse_v2/lexer.h"
#include "config/parse_v2/types.h"
#include "config/setting/dictionary.h"
#include "error.h"
#include "ui/fmt.h"
#include "util/log.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* #region Parser_LineError */

void Parser_LineError_init(Parser_LineError *line_err, enum Parser_ERR err_type, size_t lineno) {
	memset(line_err, 0, sizeof(Parser_LineError));
	line_err->type = err_type;
	line_err->line = lineno;
}

void Parser_LineError_deinit(Parser_LineError *line_err) {
	if (line_err->strerr) {
		free(line_err->strerr);
	}
}

static void Parser_LineError_Vec_init(Parser_LineError_Vec *v) {
	v->cap = 8;
	v->len = 0;
	v->data = malloc(v->cap * sizeof(Parser_LineError));
}

void Parser_LineError_Vec_deinit(Parser_LineError_Vec *v) {
	for (size_t i = 0; i < v->len; i++) {
		Parser_LineError_deinit(&v->data[i]);
	}
	free(v->data);
}

// Append a Parser_LineError. Takes ownership of error->strerr if non-NULL
static void Parser_LineError_Vec_push(Parser_LineError_Vec *v, Parser_LineError *error) {
	if (v->len == v->cap)	{
		v->cap *= 2;
		v->data = realloc(v->data, v->cap * sizeof(Parser_LineError));
	}

	Parser_LineError *dst = &v->data[v->len];
	*dst = *error;
	if (error->strerr != NULL) {
		error->strerr = NULL;
	}

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
	ParseNode node;
	ConfigVal val;
};

struct ParseNode_SettingStmt {
	ParseNode node;
	const ConfigSetting *setting;
};

struct ParseNode_KeybindStmt {
	ParseNode node;
	wchar_t keycode; // (lhs)
	bool shell; // true if this is a shell bind (shbind)
};

struct ParseNode_FnCallExpr {
	ParseNode node;
	const ConfigFn *fn;
};

// Get the true (full) size of a ParseNode using its type ID
static const size_t ParseNode_size(enum ParseNodeID type_id) {
	switch (type_id) {
	case ParseNodeID_ValueLit:
		return sizeof(ParseNode_ValueLit);
	case ParseNodeID_Root:
		return sizeof(ParseNode_Root);
	case ParseNodeID_SettingStmt:
		return sizeof(ParseNode_SettingStmt);
	case ParseNodeID_ShellStmt:
		return sizeof(ParseNode_ShellStmt);
	case ParseNodeID_KeybindStmt:
		return sizeof(ParseNode_KeybindStmt);
	case ParseNodeID_FnCallList:
		return sizeof(ParseNode_FnCallList);
	case ParseNodeID_FnCallExpr:
		return sizeof(ParseNode_FnCallExpr);
	case ParseNodeID_ArgList:
		return sizeof(ParseNode_ArgList);
	case ParseNodeID_ArgExpr:
		return sizeof(ParseNode_ArgExpr);
	}
	return 0;
}

ParseNode *ParseNode_new(enum ParseNodeID type_id) {
	const size_t true_size = ParseNode_size(type_id);
	ParseNode *node = malloc(true_size);
	memset(node, 0, true_size);
	node->type = type_id;
	return node;
}

void ParseNode_rfree(ParseNode *node) {
	static int depth = 0;
#ifdef MPL_PARSING_DEBUG
	LOG(Verbosity_DEBUG, "ParseNode_rfree called on a %s (depth = %d)\n", ParseNodeID_name(node->type), depth);
#else
	(void)depth;
#endif
	// NOTE: this is a DFS lol
	// Free children
	if (node->child != NULL) {
		depth++;
		ParseNode_rfree(node->child);
		depth--;
	}
	// Free siblings
	if (node->sibling != NULL) {
		ParseNode_rfree(node->sibling);
	}

	// Free any extra data the node holds
	switch (node->type) {
	case ParseNodeID_ValueLit:
		{
			// For some reason the below cast triggers a false positive
			// for indexing an array outside its bounds
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
			ParseNode_ValueLit *value_node = (ParseNode_ValueLit *)node;
			if (value_node->val.type == Config_STR) {
				free(value_node->val.val_str);
			}
#pragma GCC diagnostic pop
		}
		break;
	default:
		break;
	}

	// Free the node itself
	free(node);
}

ParseNode *ParseNode_rcopy(const ParseNode *node) {
	// Copy the node itself
	ParseNode *copy = ParseNode_new(node->type);
	memcpy(copy, node, ParseNode_size(node->type));

	// NOTE: this is still a DFS lol
	// Copy children
	if (node->child) {
		copy->child = ParseNode_rcopy(node->child);
	}
	// Copy siblings
	if (node->sibling) {
		copy->sibling = ParseNode_rcopy(node->sibling);
	}

	return copy;
}

// Encode arguments for a callable parse tree
static enum Parser_ERR ParseNode_FnCallExpr_encode_args(ParseNode_FnCallExpr *node, unsigned char **args_buf, ConfigRegister *ret);

enum Parser_ERR ParseNode_FnCallExpr_eval(ParseNode *node, ConfigRegister *ret) {
	if (node->type != ParseNodeID_FnCallExpr) {
		return Parser_INVALID_NODE;
	}
	ParseNode_FnCallExpr *fn_expr = (ParseNode_FnCallExpr *)node;

	// Encode argument struct for the function
	void *args = NULL;
	if (fn_expr->fn->argc > 0) {
		enum Parser_ERR err = ParseNode_FnCallExpr_encode_args(fn_expr, (unsigned char **)&args, ret);
		if (err != Parser_OK) {
			return err;
		}
	}

	// Clear return register
	ConfigRegister_clear(ret);

	// Get the function pointer and call it
	const enum ConfigType ret_type = fn_expr->fn->ret_type;
	if (ret_type != Config_VOID) {
		ret->val.type = ret_type;
		void *ret_val = fn_expr->fn->routine(args);
		memcpy(&ret->val.val_i32, &ret_val, ConfigType_size(ret->val.type));
	} else {
		fn_expr->fn->routine(args);
	}

	// Cleanup and store result
	free(args);

	return Parser_OK;
}

static enum Parser_ERR ParseNode_FnCallExpr_encode_args(ParseNode_FnCallExpr *fn_expr, unsigned char **args_buf, ConfigRegister *ret) {
	const ConfigFn *fn = fn_expr->fn;
	if (fn->argc == 0) {
		return Parser_OK;
	}
	if (!(fn_expr->node.child && fn_expr->node.child->type == ParseNodeID_ArgList)) {
		return Parser_INVALID_ARG_COUNT;
	}

	// Compute total size of function arguments
	size_t args_size = 0;
	size_t arg_maxsize = 0; // size of the largest argument
	for (size_t i = 0; i < fn->argc; i++) {
		const size_t arg_size = ConfigType_size(fn->arg_types[i]);
		if (arg_size > arg_maxsize) {
			arg_maxsize = arg_size;
		}
		args_size += arg_size;
	}
#ifdef KNOWN_STRUCT_PADDING
	// Align struct to a multiple of its largest member's size
	size_t align_rem = args_size % arg_maxsize;
	if (align_rem > 0) {
		args_size += arg_maxsize - align_rem;
	}
#endif

	*args_buf = malloc(args_size);
	size_t offset = 0; // byte offset in args_buf

	ParseNode *arg_node = fn_expr->node.child->child;
	size_t i = 0;
	for (; arg_node != NULL; arg_node = arg_node->sibling, i++) {
		if (!arg_node->child) {
			return Parser_SYNTAX_ERR;
		}

		// Handle value literal arguments
		if (arg_node->child->type == ParseNodeID_ValueLit) {
			ConfigVal *arg_val = &((ParseNode_ValueLit *)arg_node->child)->val;
			if (arg_val->type != fn->arg_types[i]) {
				return Parser_TYPE_ERR;
			}
#ifdef KNOWN_STRUCT_PADDING
			// Align member to a multiple of its size
			const size_t arg_size = ConfigType_size(arg_val->type);
			align_rem = offset % arg_size;
			if (align_rem > 0) {
				offset += arg_size - align_rem;
			}
#endif
			// unions let us skip a switch here
			memcpy(&(*args_buf)[offset], &arg_val->val_i32, ConfigType_size(arg_val->type));
			offset += ConfigType_size(arg_val->type);
			continue;
		}

		// Handle arguments that result from a function call
		if (arg_node->child->type != ParseNodeID_FnCallExpr) {
			return Parser_INVALID_NODE;
		}
		ParseNode_FnCallExpr *arg_fn = (ParseNode_FnCallExpr *)arg_node->child;
		if (arg_fn->fn->ret_type != fn->arg_types[i]) {
			return Parser_TYPE_ERR;
		}
		enum Parser_ERR err = ParseNode_FnCallExpr_eval(&arg_fn->node, ret);
		if (err != Parser_OK) {
			return err;
		}
		memcpy(&(*args_buf)[offset], &ret->val.val_i32, ConfigType_size(ret->val.type));
		offset += ConfigType_size(ret->val.type);
	}

	return Parser_OK;
}

const ConfigFn *ParseNode_FnCallExpr_get_fn(ParseNode *fn_expr) {
	if (fn_expr->type != ParseNodeID_FnCallExpr) {
		return NULL;
	}
	return ((ParseNode_FnCallExpr *)fn_expr)->fn;
}

/* #endregion */

/* #region Parsing */

struct Parser {
	Lexer *lex;
	ConfigFnDict *fn_dict;
	ConfigSettingDict *setting_dict;
	Memory *mem; // Contains ConfigFn ret register

	// Used to pass rich error messages from Parser_parse_node
	char strerr[255];
	// Used to pass the type of node where an error occured from Parser_parse_node
	enum ParseNodeID cur_node_type;
};


Parser *Parser_new(Lexer *l, ConfigFnDict *fn_dict, ConfigSettingDict *setting_dict, Memory *mem) {
	Parser *p = malloc(sizeof(Parser));
	CHECK_ALLOC(p, NULL);
	memset(p, 0, sizeof(Parser));
	p->lex = l;
	p->fn_dict = fn_dict;
	p->setting_dict = setting_dict;
	p->mem = mem;

	return p;
}
// Deinitialize and free a config parser
void Parser_free(Parser *p) {
	free(p);
}

// Parse a ParseNode. Makes recursive calls as needed.
static enum Parser_ERR Parser_parse_node(Parser *p, ParseNode *node);

ParseNode *Parser_parse(Parser *p, Parser_LineError_Vec **errors) {
	ParseNode_Root *root = ParseNode_new(ParseNodeID_Root);

	// Initialize error vector
	*errors = malloc(sizeof(Parser_LineError_Vec));
	Parser_LineError_Vec_init(*errors);

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

			case Tok_ShBind:
			case Tok_Bind:
				child = ParseNode_new(ParseNodeID_ShellStmt);
				break;

			case Tok_Ident:
				if (ConfigFnDict_has(p->fn_dict, tok->ident)) {
					child = ParseNode_new(ParseNodeID_ShellStmt);
				} else if (ConfigSettingDict_has(p->setting_dict, tok->ident)) {
					child = ParseNode_new(ParseNodeID_SettingStmt);
				} else {
					Parser_LineError line_err;
					Parser_LineError_init(&line_err, Parser_INVALID_IDENT, lineno);
					Parser_LineError_Vec_push(*errors, &line_err);
				}
				break;

			default:
				{
					Parser_LineError line_err;
					Parser_LineError_init(&line_err, Parser_SYNTAX_ERR, lineno);
					Parser_LineError_Vec_push(*errors, &line_err);
				}
		}

		if (!child) {
			Lexer_consume(p->lex);
			continue;
		}

		// Parse the child node
		enum Parser_ERR err = Parser_parse_node(p, child);
		if (err != Parser_OK) {
			// Add parser error to list
			Parser_LineError line_err;
			Parser_LineError_init(&line_err, err, lineno);
			if (p->strerr[0]) {
				line_err.strerr = strdup(p->strerr);
				p->strerr[0] = '\0';
			}
			Parser_LineError_Vec_push(*errors, &line_err);

			// Free child node with possibly invalid state
			ParseNode_rfree(child);

			// Advance to next line to avoid extraneous errors
			for (tok = Lexer_peek(p->lex); tok && tok->type != Tok_LF; tok = Lexer_peek(p->lex)) {
				Lexer_consume(p->lex);
			}
			continue;
		}

		// Add valid child node
		if (tail) {
			tail->sibling = child;
		} else {
			root->child = child;
		}
		tail = child;
	}

	return root;
}

ParseNode *Parser_parse_ShellStmt(Parser *p, Parser_LineError *err) {
	static const size_t strerr_len = sizeof(p->strerr) / sizeof(p->strerr[0]);

	// Initialize err
	memset(err, 0, sizeof(Parser_LineError));
	err->type = Parser_OK;

	ParseNode_ShellStmt *stmt = ParseNode_new(ParseNodeID_ShellStmt);

	for (const LexerToken *tok = Lexer_peek(p->lex); tok != NULL; tok = Lexer_peek(p->lex)) {
		switch (tok->type) {
			case Tok_LF:
			case Tok_Semi:
				continue;

			case Tok_ShBind:
			case Tok_Bind:
				err->type = Parser_parse_node(p, stmt);
				break;

			case Tok_Ident:
				if (ConfigFnDict_has(p->fn_dict, tok->ident)) {
					err->type =  Parser_parse_node(p, stmt);
				} else if (ConfigSettingDict_has(p->setting_dict, tok->ident)) {
					snprintf(p->strerr, strerr_len, "Settings cannot be directly edited from the shell (%s)", tok->ident);
					err->strerr = strdup(p->strerr);
					p->strerr[0] = '\0';
					err->type = Parser_INVALID_NODE;
				} else {
					err->type = Parser_SYNTAX_ERR;
				}
				break;

			default:
				err->type = Parser_SYNTAX_ERR;
		}

		if (err->type != Parser_OK) {
			// Flush lexer to avoid locking up the shell
			while (Lexer_peek(p->lex)) {
				Lexer_consume(p->lex);
			}
			break;
		}
	}


	return stmt;
}


static enum Parser_ERR Parser_parse_node(Parser *p, ParseNode *node) {
	static const size_t strerr_len = sizeof(p->strerr) / sizeof(p->strerr[0]);

	p->cur_node_type = node->type;
	LexerToken *tok = Lexer_peek(p->lex);

#ifdef MPL_PARSING_DEBUG
	LOG(Verbosity_DEBUG, "Parsing node: %s\n", ParseNodeID_name(node->type));
#endif

	switch (node->type) {
	case ParseNodeID_Root:
		{
			// Use parser_parse instead
			return Parser_INVALID_NODE;
		}
		break;

	case ParseNodeID_ValueLit:
		{
			ConfigVal *val = &((ParseNode_ValueLit *)node)->val;
			switch (tok->type) {
				case Tok_I32Lit:
					val->type = Config_I32;
					val->val_i32 = tok->i32_lit;
					break;
				case Tok_StrLit:
					val->type = Config_STR;
					val->val_str = tok->str_lit;
					tok->str_lit = NULL;
					break;
				case Tok_BoolLit:
					val->type = Config_BOOL;
					val->val_bool = tok->bool_lit;
					break;
				default:
					return Parser_SYNTAX_ERR;
			}
			Lexer_consume(p->lex);
		}
		break;
	
	case ParseNodeID_SettingStmt:
		{
			// {ident}
			ParseNode_SettingStmt *stmt = (ParseNode_SettingStmt *)node;
			ConfigSettingDict_lookup(p->setting_dict, &stmt->setting, tok->ident);
			Lexer_consume(p->lex);
			if (!stmt->setting) {
				return Parser_INVALID_SETTING;
			}

			// =
			if (Lexer_peek(p->lex)->type != Tok_EQ){
				return Parser_SYNTAX_ERR;
			}
			Lexer_consume(p->lex);

			// {val}
			node->child = ParseNode_new(ParseNodeID_ValueLit);
			const enum Parser_ERR err = Parser_parse_node(p, node->child);
			if (err != Parser_OK) {
				return err;
			}

			// Check setting type
			ConfigVal *val = &((ParseNode_ValueLit *)node->child)->val;
			if (val->type != stmt->setting->type) {
				snprintf(p->strerr, strerr_len, "Setting %s has type %s but %s was passed",
						stmt->setting->ident, ConfigType_name(stmt->setting->type), ConfigType_name(val->type));
				return Parser_TYPE_ERR;
			}
		}
		break;

	case ParseNodeID_ShellStmt:
		{
			switch (tok->type) {
			case Tok_ShBind:
			case Tok_Bind:
				{
					node->child = ParseNode_new(ParseNodeID_KeybindStmt);
					if (tok->type == Tok_ShBind) {
						((ParseNode_KeybindStmt *)node->child)->shell = true;
					}
					return Parser_parse_node(p, node->child);
				}
			case Tok_Ident:
				{
					if (ConfigFnDict_has(p->fn_dict, tok->ident)) {
						node->child = ParseNode_new(ParseNodeID_FnCallList);
						return Parser_parse_node(p, node->child);
					} else {
						return Parser_INVALID_FUNCTION;
					}
				}
			default:
				return Parser_SYNTAX_ERR;
			}
		}
		break;

	case ParseNodeID_KeybindStmt:
		{
			ParseNode_KeybindStmt *stmt = (ParseNode_KeybindStmt *)node;
			// bind / shbind
			if (!(tok->type == Tok_Bind || tok->type == Tok_ShBind)) {
				return Parser_SYNTAX_ERR;
			}
			stmt->shell = tok->type == Tok_ShBind;
			Lexer_consume(p->lex);

			// {keysym}
			tok = Lexer_peek(p->lex);
			if (tok->type != Tok_Keysym) {
				return Parser_SYNTAX_ERR;
			}
			stmt->keycode = tok->keycode;
			Lexer_consume(p->lex);

			// =
			if (Lexer_peek(p->lex)->type != Tok_EQ) {
				return Parser_SYNTAX_ERR;
			}
			Lexer_consume(p->lex);

			// {FnCallList}
			node->child = ParseNode_new(ParseNodeID_FnCallList);
			return Parser_parse_node(p, node->child);
		}
		break;

	case ParseNodeID_FnCallList:
		{
			if (tok->type != Tok_Ident) {
				LOG(Verbosity_DEBUG, "in FnCallList: tok->type = %s\n", LexerToken_t_name(tok->type));
				return Parser_SYNTAX_ERR;
			}

			ParseNode_FnCallExpr *tail = NULL;
			while (tok && tok->type == Tok_Ident) {
				// Parse FnCallExpr
				ParseNode *fn_call = ParseNode_new(ParseNodeID_FnCallExpr);
				enum Parser_ERR err = Parser_parse_node(p, fn_call);
				if (err != Parser_OK) {
					ParseNode_rfree(fn_call);
					return err;
				}

				// Append the fn call to the list
				if (tail) {
					tail->node.sibling = fn_call;
				} else {
					node->child = fn_call;
				}
				tail = (ParseNode_FnCallExpr *)fn_call;

				// Consume delim
				while (Lexer_peek(p->lex) && Lexer_peek(p->lex)->type == Tok_Semi) {
					Lexer_consume(p->lex);
				}
				tok = Lexer_peek(p->lex);
			}
		}
		break;
	
	case ParseNodeID_FnCallExpr:
		{
			ParseNode_FnCallExpr *expr = (ParseNode_FnCallExpr *)node;
			// {ident}
			if (tok->type != Tok_Ident) {
				return Parser_SYNTAX_ERR;
			}
			ConfigFnDict_lookup(p->fn_dict, &expr->fn, tok->ident);
			Lexer_consume(p->lex);
			if (!expr->fn) {
				return Parser_INVALID_FUNCTION;
			}

			// (
			tok = Lexer_peek(p->lex);
			if (tok->type != Tok_Lparen) {
				return Parser_SYNTAX_ERR;
			}
			Lexer_consume(p->lex);

			// ArgList
			if (expr->fn->argc > 0) {
				expr->node.child = ParseNode_new(ParseNodeID_ArgList);
				ParseNode_ArgList *arg_list = (ParseNode_ArgList *)expr->node.child;
				enum Parser_ERR err = Parser_parse_node(p, expr->node.child);
				if (err != Parser_OK) {
					return err;
				}

				// Check arg count
				ParseNode *arg = arg_list->child;
				size_t args_passed = 0;
				for (; arg != NULL; arg = arg->sibling) {
					args_passed++;
				}
				if (args_passed != expr->fn->argc) {
					snprintf(p->strerr, strerr_len, "Function %s accepts %zu arg(s) but %zu arg(s) were passed",
							expr->fn->ident, expr->fn->argc, args_passed);
					return Parser_INVALID_ARG_COUNT;
				}

				// Check arg types
				arg = arg_list->child;
				for (size_t i = 0; i < expr->fn->argc; i++, arg = arg->sibling) {
					// Deduce arg type
					enum ConfigType arg_type;
					switch (arg->child->type) {
					case ParseNodeID_ValueLit:
						arg_type = ((ParseNode_ValueLit *)arg->child)->val.type;
						break;
					case ParseNodeID_FnCallExpr:
						arg_type = ((ParseNode_FnCallExpr *)arg->child)->fn->ret_type;
						break;
					default:
						return Parser_INVALID_NODE;
					}
					// Ensure arg has correct type
					const enum ConfigType correct_arg_type = expr->fn->arg_types[i];
					if (arg_type != correct_arg_type) {
						snprintf(p->strerr, strerr_len, "Function %s argument %zu has type %s but %s was passed",
								expr->fn->ident, i, ConfigType_name(correct_arg_type), ConfigType_name(arg_type));
						return Parser_TYPE_ERR;
					}
				}
			}

			// )
			tok = Lexer_peek(p->lex);
			if (!tok || tok->type != Tok_Rparen) {
				return Parser_SYNTAX_ERR;
			}
			Lexer_consume(p->lex);
		}
		break;

	case ParseNodeID_ArgList:
		{
			ParseNode_ArgExpr *tail = NULL;
			while (Lexer_peek(p->lex) && Lexer_peek(p->lex)->type != Tok_Rparen) {
				// ArgExpr
				ParseNode *arg = ParseNode_new(ParseNodeID_ArgExpr);
				enum Parser_ERR err = Parser_parse_node(p, arg);
				if (err != Parser_OK) {
					ParseNode_rfree(arg);
					return err;
				}

				// Append the arg to the list
				if (tail) {
					tail->sibling = arg;
				} else {
					node->child = arg;
				}
				tail = (ParseNode_ArgExpr *)arg;

				// Consume delim 
				tok = Lexer_peek(p->lex);
				if (tok && tok->type == Tok_Comma) {
					Lexer_consume(p->lex);
				}
			}
		}
		break;

	case ParseNodeID_ArgExpr:
		{
			switch (tok->type) {
			case Tok_I32Lit:
			case Tok_BoolLit:
			case Tok_StrLit:
				{
					node->child = ParseNode_new(ParseNodeID_ValueLit);
					return Parser_parse_node(p, node->child);
				}
				break;

			case Tok_Ident:
				{
					node->child = ParseNode_new(ParseNodeID_FnCallExpr);
					return Parser_parse_node(p, node->child);
				}
				break;

			default:
				return Parser_SYNTAX_ERR;
			}
		}
	}

	return Parser_OK;
}

/* #endregion */

/* #region Parse tree traversal */


enum Parser_ERR Parser_walk(Parser *p, Config *config, ParserWalkFlags flags, ParseNode *tree) {
	switch (tree->type)	{
	case ParseNodeID_Root:
		{
			for (ParseNode *child = tree->child; child != NULL; child = child->sibling) {
				enum Parser_ERR err = Parser_walk(p, config, flags, child);
				if (err != Parser_OK) {
					return err;
				}
			}
		}
		break;

	case ParseNodeID_SettingStmt:
		{
			if (!(flags & Parser_WALK_SETTINGS)) {
				return Parser_OK;
			}

			ParseNode_SettingStmt *stmt = (ParseNode_SettingStmt *)tree;
			// Validate we have a value for the setting
			ParseNode_ValueLit *val_node = (ParseNode_ValueLit *)stmt->node.child;
			if (!val_node || val_node->node.type != ParseNodeID_ValueLit) {
				return Parser_SYNTAX_ERR;
			}
			// Validate the value matches the setting's type
			ConfigVal *val = &val_node->val;
			if (val->type != stmt->setting->type) {
				// type mismatch
				return Parser_TYPE_ERR;
			}

			void *dst = (unsigned char *)(&config->settings) + stmt->setting->struct_offset;
			switch (val->type) {
			case Config_I32:
				*(int32_t *)dst = val->val_i32;
				break;

			case Config_STR:
				*(char **)dst = strdup(val->val_str);
				break;

			case Config_BOOL:
				*(bool *)dst = val->val_bool;
				break;

			// We cannot have literals of other types
			default:
				return Parser_TYPE_ERR;
			}
		}
		break;
	case ParseNodeID_ShellStmt:
		{
			enum Parser_ERR err = Parser_walk(p, config, flags, tree->child);
			if (err != Parser_OK) {
				return err;
			}

			// If we have an eval result, print it using fmt_data
			const ConfigRegister *ret = &p->mem->ret;
			if (ret->val.type != Config_VOID)  {
				// TODO remove hardcoded formatter
				fmt_data(&FMT_CLI, &ret->val);
			}
		}
		break;
	case ParseNodeID_KeybindStmt:
		{
			if (!(flags & Parser_WALK_KEYBINDS)) {
				return Parser_OK;
			}

			ParseNode_KeybindStmt *stmt = (ParseNode_KeybindStmt *)tree;
			enum Keybind_ERR err = KeybindMap_define_keybind(config->keybinds, stmt->keycode, tree->child, stmt->shell);
			if (err != Keybind_OK) {
				return Parser_KEYBIND_ERR;
			}
		}
		break;

	case ParseNodeID_FnCallList:
		{
			if (!(flags & Parser_WALK_FUNCTIONS || flags & Parser_WALK_MACROS)) {
				return Parser_OK;
			}

			for (ParseNode *fn_expr = tree->child; fn_expr != NULL; fn_expr = fn_expr->sibling) {
				enum Parser_ERR err = Parser_walk(p, config, flags, fn_expr);
				if (err != Parser_OK) {
					return err;
				}
			}

		}
		break;
	case ParseNodeID_FnCallExpr:
		{
			ParseNode_FnCallExpr *fn_expr = (ParseNode_FnCallExpr *)tree;
			const ConfigFn *fn = fn_expr->fn;
			if ((fn->is_macro && !(flags & Parser_WALK_MACROS)) ||
					(!fn->is_macro && !(flags & Parser_WALK_FUNCTIONS))) {
				return Parser_OK;
			}

			enum Parser_ERR err = ParseNode_FnCallExpr_eval(tree, &p->mem->ret);

			return err;
		}
		break;
	
	case ParseNodeID_ValueLit:
	case ParseNodeID_ArgList:
	case ParseNodeID_ArgExpr:
		return Parser_INVALID_NODE;
	}

	return Parser_OK;
}
