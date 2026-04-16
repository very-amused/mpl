#include "register.h"
extern "C" {
#include "dictionary.h"
#include "error.h"
#include "function.h"

#include <string.h>
}


#include <string>
#include <unordered_map>
#include <memory>

ConfigFn::~ConfigFn() {
	ConfigFn_deinit(this);
}

extern "C" {

struct ConfigFnDict {
	std::unordered_map<std::string, std::unique_ptr<ConfigFn>> map;
};

ConfigFnDict *ConfigFnDict_new() {
	return new ConfigFnDict();
}

void ConfigFnDict_free(ConfigFnDict *dict) {
	delete dict;
}

enum ConfigFn_ERR ConfigFnDict_lookup(ConfigFnDict *dict, const ConfigFn **dst,
		const char *ident) {
	const bool exists = dict->map.find(ident) != dict->map.end();
	if (!exists) {
		return ConfigFn_INVALID_FN;
	}

	*dst = dict->map[ident].get();

	return ConfigFn_OK;
}

}


void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void (*routine)(void *args),
		enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state),
		void (*free_args)(void *args)) {
	std::unique_ptr<ConfigFn> fn = std::unique_ptr<ConfigFn>(new ConfigFn);
	fn->ident = strdup(ident);
	fn->is_macro = false;
	fn->routine = routine;
	fn->parse_args = parse_args;
	fn->free_args = free_args;

	dict->map[std::string(fn->ident)] = std::move(fn);
}

void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void (*routine)(void *args),
		enum ConfigFn_ERR (*parse_args)(void **args, StrtoknState *parse_state),
		void (*free_args)(void *args)) {
	std::unique_ptr<ConfigFn> macro = std::unique_ptr<ConfigFn>(new ConfigFn);
	macro->ident = strdup(ident);
	macro->is_macro = true;
	macro->routine = routine;
	macro->parse_args = parse_args;
	macro->free_args = free_args;

	dict->map[std::string(macro->ident)] = std::move(macro);
}
