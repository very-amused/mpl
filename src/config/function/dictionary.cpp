extern "C" {
#include "dictionary.h"
#include "function.h"

#include <string.h>
#include <stdlib.h>
}

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

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

void ConfigFnDict_lookup(ConfigFnDict *dict, const ConfigFn **dst, const char *ident) {
	*dst = NULL;
	const bool exists = dict->map.find(ident) != dict->map.end();
	if (!exists) {
		return;
	}

	*dst = dict->map[ident].get();
}

const bool ConfigFnDict_has(ConfigFnDict *dict, const char *ident) {
	return dict->map.find(ident) != dict->map.end();
}

}

void ConfigFnDict_define_fn(ConfigFnDict *dict, const char *ident,
		void *(*routine)(void *args),
		const enum ConfigType ret_type,
		std::vector<enum ConfigType> arg_types) {
	std::unique_ptr<ConfigFn> fn = std::unique_ptr<ConfigFn>(new ConfigFn);
	fn->ident = strdup(ident);
	fn->is_macro = false;


	// Handle arg types and return type
	fn->arg_types = NULL;
	fn->argc = arg_types.size();
	if (fn->argc > 0) {
		fn->arg_types = (enum ConfigType *)malloc(fn->argc * sizeof(enum ConfigType));
		for (size_t i = 0; i < fn->argc; i++) {
			fn->arg_types[i] = arg_types[i];
		}
	}
	fn->ret_type = ret_type;

	fn->routine = routine;

	dict->map[std::string(fn->ident)] = std::move(fn);
}

void ConfigFnDict_define_macro(ConfigFnDict *dict, const char *ident,
		void *(*routine)(void *args),
		const enum ConfigType ret_type,
		const enum ConfigType *arg_types, const size_t arg_count) {
	std::unique_ptr<ConfigFn> macro = std::unique_ptr<ConfigFn>(new ConfigFn);
	macro->ident = strdup(ident);
	macro->is_macro = true;

	// Handle arg types and return type
	macro->arg_types = NULL;
	macro->argc = arg_count;
	if (arg_count > 0) {
		macro->arg_types = (enum ConfigType *)malloc(macro->argc * sizeof(enum ConfigType));
		for (size_t i = 0; i < macro->argc; i++) {
			macro->arg_types[i] = arg_types[i];
		}
	}
	macro->ret_type = ret_type;

	macro->routine = routine;

	dict->map[std::string(macro->ident)] = std::move(macro);
}

