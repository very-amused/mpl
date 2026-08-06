#include "memory.h"
#include "config/parse_v2/types.h"
#include "error.h"
#include "util/log.h"

#include <stdlib.h>
#include <string.h>

void MemRegister_init(MemRegister *r, const char *name) {
#ifdef MPL_MEM_DEBUG
	LOG(Verbosity_DEBUG, "Initializing MemRegister (%s)\n", name);
#endif
	r->name = name;
	r->val.type = Config_VOID;
	r->is_owned = false;
}

void MemRegister_clear(MemRegister *r) {
	if (r->is_owned) {
		free(r->val.val_ptr);
	}
	r->val.type = Config_VOID;
	r->is_owned = false;
	memset(&r->val.val_ptr, 0, sizeof(void *));
#ifdef MPL_MEM_DEBUG
	LOG(Verbosity_DEBUG, "Cleared MemRegister (%s)\n", r->name);
#endif
}

ConfigVal MemRegister_load(const MemRegister *r) {
#ifdef MPL_MEM_DEBUG
	LOG(Verbosity_DEBUG, "Load (%s) -> %p\n", r->name, r->val.val_ptr);
#endif
	return r->val;
}

void MemRegister_store(MemRegister *r, ConfigVal val, bool is_owned) {
#ifdef MPL_MEM_DEBUG
	LOG(Verbosity_DEBUG, "Store %p [%s] -> (%s)\n", val.val_ptr, ConfigType_pretty_name(val.type), r->name);
#endif
	r->val = val;
	r->is_owned = is_owned;
}

void Memory_init(Memory *mem) {
	// Initialize and name registers
	MemRegister_init(&mem->ret, "ret");
}

void Memory_deinit(Memory *mem) {
	// Clear registers
	MemRegister_clear(&mem->ret);
}
