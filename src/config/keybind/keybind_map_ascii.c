#include "config/function/function.h"
#include "config/parse_v2/keycode.h"
#include "error.h"
#include "keybind_map.h"
#include "util/log.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <locale.h>

struct KeybindMap {
	ParseNode *map[256];
};

KeybindMap *KeybindMap_new() {
	KeybindMap *keybinds = malloc(sizeof(KeybindMap));
	CHECK_ALLOC(keybinds, NULL);
	memset(keybinds, 0, sizeof(KeybindMap));
	return keybinds;
}

void KeybindMap_free(KeybindMap *keybinds) {
	static const size_t MAP_LEN = sizeof(keybinds->map) / sizeof(keybinds->map[0]);
	for (size_t i = 0; i < MAP_LEN; i++) {
		if (keybinds->map[i]) {
			ParseNode_rfree(keybinds->map[i]);
		}
	}
	free(keybinds);
}

enum Keybind_ERR KeybindMap_define_keybind(KeybindMap *keybinds, wchar_t keycode, const ParseNode *fn_list) {
	// Ensure we're dealing with an ASCII keycode to prevent overflow
	if (!is_uascii(keycode)) {
		return Keybind_NON_ASCII;
	}

	if (fn_list->type != ParseNodeID_FnCallList) {
		return Keybind_INVALID_FN;
	}
	if (keybinds->map[keycode]) {
		return Keybind_BINDING_CONFLICT;
	}

	keybinds->map[keycode] = ParseNode_rcopy(fn_list);
	return Keybind_OK;
}

enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode) {
	// Ensure we're dealing with an ASCII keycode to prevent overflow
	if (!is_uascii(keycode)) {
		return Keybind_NON_ASCII;
	}

	const ParseNode *fn_list = keybinds->map[keycode];
	if (!fn_list) {
		return Keybind_NOT_FOUND;
	}

	for (ParseNode *fn_expr = fn_list->child; fn_expr != NULL; fn_expr = fn_expr->sibling) {
		enum Parser_ERR err = ParseNode_FnCallExpr_eval(fn_expr, NULL);
		if (err != Parser_OK) {
			LOG(Verbosity_NORMAL, "Failed to call keybind for key %c: %s returned %s\n",
					keycode, ParseNode_FnCallExpr_get_fn(fn_expr)->ident, Parser_ERR_name(err));
			return Keybind_CALL_ERR;
		}
	}

	return Keybind_OK;
}
