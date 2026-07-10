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
	KeybindRoutine(const ParseNode *fn_list);
	~KeybindRoutine();
} KeybindDefinition;

KeybindRoutine::KeybindRoutine(const ParseNode *fn_list) {
	this->fn_list = ParseNode_rcopy(fn_list);
}

KeybindRoutine::~KeybindRoutine() {
	ParseNode_rfree(this->fn_list);
}

/* #endregion */

/* #region KeybindMap */

struct KeybindMap {
	std::unordered_map<wchar_t, std::unique_ptr<KeybindRoutine>> map;
	std::unordered_map<wchar_t, std::unique_ptr<KeybindRoutine>> shell_map; // Keybinds used in MPL's shell
	ConfigRegister *ret; // eval return register
};

KeybindMap *KeybindMap_new(ConfigRegister *ret) {
	KeybindMap *map = new KeybindMap;
	map->ret = ret;
	return map;
}

void KeybindMap_free(KeybindMap *keybinds) {
	delete keybinds;
}

enum Keybind_ERR KeybindMap_define_keybind(KeybindMap *keybinds, wchar_t keycode, const ParseNode *fn_list, bool shell) {
	auto &map = shell ? keybinds->shell_map : keybinds->map;

	if (fn_list->type != ParseNodeID_FnCallList) {
		return Keybind_INVALID_FN;
	}
	if (map.find(keycode) != map.end()) {
		return Keybind_BINDING_CONFLICT;
	}

	std::unique_ptr<KeybindRoutine> routine(new KeybindRoutine(fn_list));
	map[keycode] = std::move(routine);
	return Keybind_OK;
}


enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode, bool shell) {
	auto &map = shell ? keybinds->shell_map : keybinds->map;

	const bool exists = map.find(keycode) != map.end();
	if (!exists) {
		return Keybind_NOT_FOUND;
	}

	KeybindRoutine *routine = map[keycode].get();
	ParseNode *fn_list = routine->fn_list;
	for (ParseNode *fn_expr = fn_list->child; fn_expr != NULL; fn_expr = fn_expr->sibling) {
		enum Parser_ERR err = ParseNode_FnCallExpr_eval(fn_expr, keybinds->ret);
		if (err != Parser_OK) {
			LOG(Verbosity_NORMAL, "Failed to call keybind for key %c: %s returned %s",
					keycode, ParseNode_FnCallExpr_get_fn(fn_expr)->ident, Parser_ERR_name(err));
			return Keybind_CALL_ERR;
		}
	}

	return Keybind_OK;
}

/* #endregion */

}
