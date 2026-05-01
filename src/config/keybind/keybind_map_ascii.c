#include "config/function/dictionary.h"
#include "config/function/function.h"
#include "keycode.h"
#include "error.h"
#include "keybind_map.h"
#include "util/log.h"
#include "util/strtokn.h"

#include <wctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <locale.h>

struct KeybindMap {
	ConfigFnCallArray *legacy_map[256];
	ParseNode *map[256];
};

KeybindMap *KeybindMap_new() {
	KeybindMap *keybinds = malloc(sizeof(KeybindMap));
	CHECK_ALLOC(keybinds, NULL);
	memset(keybinds, 0, sizeof(KeybindMap));
	return keybinds;
}

void KeybindMap_free(KeybindMap *keybinds) {
	static const size_t MAP_LEN = sizeof(keybinds->legacy_map) / sizeof(keybinds->legacy_map[0]);
	for (size_t i = 0; i < MAP_LEN; i++) {
		if (keybinds->legacy_map[i]) {
			ConfigFnCallArray_deinit(keybinds->legacy_map[i]);
		}
		free(keybinds->legacy_map[i]);
	}
	free(keybinds);
}

enum Keybind_ERR KeybindMap_define_keybind(KeybindMap *keybinds, wchar_t keycode, const ParseNode *fn_list) {
	// Ensure we're dealing with an ASCII keycode to prevent overflow
	if (keycode > (unsigned char)-1) {
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

static enum Keybind_ERR KeybindMap_call_keybind_legacy(KeybindMap *keybinds, wchar_t keycode);

enum Keybind_ERR KeybindMap_call_keybind(KeybindMap *keybinds, wchar_t keycode) {
	// Ensure we're dealing with an ASCII keycode to prevent overflow
	if (keycode > (unsigned char)-1) {
		return Keybind_NON_ASCII;
	}

	const ParseNode *fn_list = keybinds->map[keycode];
	if (!fn_list) {
		return KeybindMap_call_keybind_legacy(keybinds, keycode);
	}

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
	// Ensure we're dealing with an ASCII keycode to prevent overflow
	if (keycode > (unsigned char)-1) {
		return Keybind_NON_ASCII;
	}

	const ConfigFnCallArray *fn_call_arr = keybinds->legacy_map[keycode];
	if (!fn_call_arr) {
		return Keybind_NOT_FOUND;
	}

	// Call keybind functions
	for (size_t i = 0; i < fn_call_arr->n; i++) {
		ConfigFnCall_exec(fn_call_arr->fn_calls[i]);
	}

	return Keybind_OK;
}

enum Keybind_ERR KeybindMap_parse_mapping_legacy(KeybindMap *keybinds, const char *line, ConfigFnDict *fn_dict) {
	// Ensure utf8 support
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
	char keyname[parse_state.tok_len+1];
	strncpy(keyname, &line[parse_state.offset], parse_state.tok_len);
	keyname[parse_state.tok_len] = '\0';
	// Parse keycode
	const wchar_t keycode = parse_keycode(keyname);
	LOG(Verbosity_DEBUG, "Parsed keycode:\n\tkeyname: %s\n", keyname);
	if (iswprint(keycode)) {
		LOG(Verbosity_DEBUG, "\tkeycode: %lc\n", keycode);
	} else {
		LOG(Verbosity_DEBUG, "\tkeycode (hex): %x\n", keycode);
	}
	LOG(Verbosity_DEBUG, "\tkeycode is unsigned ASCII: %s\n", is_uascii(keycode) ? "true" : "false");
	if (!is_uascii(keycode)) {
		return Keybind_NON_ASCII;
	}

	// =
	NEXT();

#undef NEXT
	
	// {function}({args}...); {function2}({args2}...)
	ConfigFnCallArray *fn_call_arr = malloc((sizeof(ConfigFnCallArray)));
	CHECK_ALLOC(fn_call_arr, Keybind_BAD_ALLOC);
	enum ConfigFn_ERR err = ConfigFnCallArray_parse(fn_call_arr, &parse_state, fn_dict);
	if (err != ConfigFn_OK) {
		ConfigFnCallArray_deinit(fn_call_arr);
		free(fn_call_arr);
		return 1;
	}

	keybinds->legacy_map[keycode] = fn_call_arr;
	return Keybind_OK;
}
