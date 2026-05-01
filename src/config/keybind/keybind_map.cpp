#include "config/parse_v2/parser.h"
extern "C" {
#include "keybind_map.h"
#include "error.h"
#include "util/log.h"
#include "util/strtokn.h"
#include "config/function/function.h"
#include "config/function/dictionary.h"
#include "config/parse_v2/keycode.h"

#include <wctype.h>
#include <locale.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
}

#include <unordered_map>
#include <string>
#include <memory>

extern "C" {

// Better to be specific about the type of parse node we're calling
typedef ParseNode ParseNode_FnCallList;

struct KeybindMap {
	std::unordered_map<wchar_t, std::unique_ptr<ConfigFnCallArray>> legacy_map;
	std::unordered_map<wchar_t, std::unique_ptr<ParseNode>> map;
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

	keybinds->map[keycode] = std::unique_ptr<ParseNode>(ParseNode_rcopy(fn_list));
	return Keybind_OK;
}

static enum Keybind_ERR KeybindMap_call_keybind_legacy(KeybindMap *keybinds, wchar_t keycode);

enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->map.find(keycode) != keybinds->map.end();
	if (!exists) {
		return KeybindMap_call_keybind_legacy(keybinds, keycode);
	}

	ParseNode *fn_list = keybinds->map[keycode].get();
	for (ParseNode *fn_expr = fn_list->child; fn_expr != NULL; fn_expr = fn_expr->sibling) {
		enum Parser_ERR err = ParseNode_FnCallExpr_eval(fn_expr, NULL);
		if (err != Parser_OK) {
			LOG(Verbosity_NORMAL, "Failed to call keybind for key %c: %s returned %s",
					keycode, ParseNode_FnCallExpr_get_fn(fn_expr)->ident, Parser_ERR_name(err));
			return Keybind_SYNTAX_ERR;
		}
	}

	return Keybind_OK;
}

static enum Keybind_ERR KeybindMap_call_keybind_legacy(KeybindMap *keybinds, wchar_t keycode) {
	const bool exists = keybinds->legacy_map.find(keycode) != keybinds->legacy_map.end();
	if (!exists) {
		return Keybind_NOT_FOUND;
	}

	ConfigFnCallArray *fn_call_arr = keybinds->legacy_map[keycode].get();
	for (size_t i = 0; i < fn_call_arr->n; i++) {
		ConfigFnCall_exec(fn_call_arr->fn_calls[i]);
	}

	return Keybind_OK;
}

enum Keybind_ERR KeybindMap_parse_mapping_legacy(KeybindMap *keybinds, const char *line, ConfigFnDict *fn_dict) {
	// Ensure UTF-8 support
	setlocale(LC_ALL, "");
	// Split by whitespace
	StrtoknState parse_state;
	strtokn_init(&parse_state, line, strlen(line));
	static const char DELIMS[] = " \n\t\r";

#define NEXT() if (strtokn(&parse_state, DELIMS) != 0) return Keybind_SYNTAX_ERR
	// bind
	NEXT();
	if (strncmp(&line[parse_state.offset], "bind", parse_state.tok_len) != 0) {
		return Keybind_SYNTAX_ERR;
	}

	// {keyname}
	NEXT();
	std::string keyname(&line[parse_state.offset], parse_state.tok_len);
	const wchar_t keycode = parse_keycode(keyname.c_str());
	LOG(Verbosity_DEBUG, "Parsed keycode:\n\tkeyname: %s\n", keyname.c_str());
	if (iswprint(keycode)) {
		LOG(Verbosity_DEBUG, "\tkeycode: %lc\n", keycode);
	} else {
		LOG(Verbosity_DEBUG, "\tkeycode (hex): %x\n", keycode);
	}
	LOG(Verbosity_DEBUG, "\tkeycode is unsigned ASCII: %s\n", is_uascii(keycode) ? "true" : "false");

	// =
	NEXT();

	// Construct our function call array for this keybind
	std::unique_ptr<ConfigFnCallArray> fn_call_arr(new ConfigFnCallArray);
	enum ConfigFn_ERR err = ConfigFnCallArray_parse(fn_call_arr.get(), &parse_state, fn_dict);
	if (err != ConfigFn_OK) {
	}

	// Add the routine to our keybind map
	keybinds->legacy_map[keycode] = std::move(fn_call_arr);

	return Keybind_OK;
}


}
