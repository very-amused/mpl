#include "lexer.h"
#include "error.h"
#include "util/log.h"

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

void confTokenNode_dll_init(confTokenNode *head) {
	head->token = NULL;
	head->prev = head->next = head;
}
// Deinitialize a DLL holding confToken's, calling confToken_deinit() on all tokens
void confTokenNode_dll_deinit(confTokenNode *head) {
	// Pop and free any remaining tokens
	confToken *tok = confTokenList_pop(head);
	for (; tok != NULL; tok = confTokenList_pop(head)) {
		LOG(Verbosity_VERBOSE, "Unpopped config token (type %s)\n", confToken_t_name(tok->token_t));
		confToken_deinit(tok);
		free(tok);
	}

	// Caller is responsible for free(head)
}

int confTokenList_push(confTokenNode *head, confToken *token) {
	confTokenNode *node = malloc(sizeof(confTokenNode));
	if (!node) {
		return 1;
	}
	node->token = token;

	// Prepare for link-in
	node->prev = head->prev;
	node->next = head;

	// Link in
	head->prev->next = node;
	head->prev = node;
	return 0;
}

confToken *confTokenList_pop(confTokenNode *head) {
	confTokenNode *front = head->next;
	if (front == head) {
		return NULL;
	}

	// Link out
	head->next = front->next;
	head->next->prev = head;

	confToken *token = front->token;
	free(front);

	return token;
}

const confToken *confTokenList_peek(const confTokenList *head) {
	confTokenNode *front = head->next;
	return front != head ? front->token : NULL;
}
