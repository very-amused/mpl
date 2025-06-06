#include "lexer.h"

#include <stdlib.h>

void confToken_deinit(confToken *tok) {
	switch (tok->token_t) {
	case confToken_STR_LIT:
		free(tok->val.str_lit);
		break;
	
	case confToken_GLOBAL_IDENT:
	case confToken_FN_IDENT:
	case confToken_VAR_IDENT:
	case confToken_KEYBOARD_IDENT:
		free(tok->val.ident_name);
		break;
	
	default:
		break;
	}
}

void confTokenNode_init_dll(confTokenNode *head) {
	head->token = NULL;
	head->prev = head->next = head;
}
// Deinitialize a DLL holding confToken's, calling confToken_deinit() on all tokens
void confTokenNode_deinit_dll(confTokenNode *head) {
	// TODO
}
