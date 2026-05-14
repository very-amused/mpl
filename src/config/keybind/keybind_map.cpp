extern "C" {
#include "keybind_map.h"
#include "error.h"
#include "util/log.h"
#include "config/parse_v2/parser.h"
#include "config/function/function.h"

#include <locale.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
}

#include <unordered_map>
#include <memory>

extern "C" {

/* #region KeybindRoutine */

// Better to be specific about the type of parse node we're calling
typedef ParseNode ParseNode_FnCallList;

typedef struct KeybindRoutine {
	ParseNode *fn_list;
	bool shell_enabled;
	KeybindRoutine(const ParseNode *fn_list);
	~KeybindRoutine();
} KeybindDefinition;

KeybindRoutine::KeybindRoutine(const ParseNode *fn_list) {
	this->fn_list = ParseNode_rcopy(fn_list);
	this->shell_enabled = ParseNode_FnCallList_is_shell_enabled(fn_list);
}

KeybindRoutine::~KeybindRoutine() {
	ParseNode_rfree(this->fn_list);
}

/* #endregion */

/* #region KeybindMap */

struct KeybindMap {
	std::unordered_map<wchar_t, std::unique_ptr<KeybindRoutine>> map;
};

KeybindMap *KeybindMap_new() {
	return new KeybindMap;
}

void KeybindMap_free(KeybindMap *keybinds) {
	delete keybinds;
}

enum Keybind_ERR KeybindMap_define_keybind(KeybindMap *keybinds, wchar_t keycode, const ParseNode *fn_list) {
	if (fn_list->type != ParseNodeID_FnCallList) {
		return Keybind_INVALID_FN;
	}
	if (keybinds->map.find(keycode) != keybinds->map.end()) {
		return Keybind_BINDING_CONFLICT;
	}

	std::unique_ptr<KeybindRoutine> routine(new KeybindRoutine(fn_list));
	keybinds->map[keycode] = std::move(routine);
	return Keybind_OK;
}


enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	if (!exists) {
		return Keybind_NOT_FOUND;
	}

	KeybindRoutine *routine = keybinds->map[keycode].get();
	ParseNode *fn_list = routine->fn_list;
	for (ParseNode *fn_expr = fn_list->child; fn_expr != NULL; fn_expr = fn_expr->sibling) {
		enum Parser_ERR err = ParseNode_FnCallExpr_eval(fn_expr, NULL);
		if (err != Parser_OK) {
			LOG(Verbosity_NORMAL, "Failed to call keybind for key %c: %s returned %s",
					keycode, ParseNode_FnCallExpr_get_fn(fn_expr)->ident, Parser_ERR_name(err));
			return Keybind_CALL_ERR;
		}
	}

	return Keybind_OK;
}

enum Keybind_ERR KeybindMap_call_shell_keybind(KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	if (!exists) {
		return Keybind_NOT_FOUND;
	}

	KeybindRoutine *routine = keybinds->map[keycode].get();
	if (!routine->shell_enabled) {
		return Keybind_NOT_FOUND;
	}

	return KeybindMap_call_keybind(keybinds, keycode);
}

/* #endregion */

}
